/*
 * Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
 * Twine is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Twine is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with Twine.
 * If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief Worker pool implementation
 * @copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef TWINE_WORKER_POOL_IMPLEMENTATION_H
#define TWINE_WORKER_POOL_IMPLEMENTATION_H

#include <cassert>
#include <atomic>
#include <vector>
#include <array>
#include <cstring>
#include <cerrno>

#include "thread_helpers.h"
#include "twine_internal.h"

namespace twine {

void set_flush_denormals_to_zero();

inline void enable_break_on_mode_sw()
{
    pthread_setmode_np(0, PTHREAD_WARNSW, 0);
}

inline WorkerPoolStatus errno_to_worker_status(int error)
{
    switch (error)
    {
        case 0:
            return WorkerPoolStatus::OK;

        case EAGAIN:
            return WorkerPoolStatus::LIMIT_EXCEEDED;

        case EPERM:
            return WorkerPoolStatus::PERMISSION_DENIED;

        default:
            return WorkerPoolStatus::ERROR;
    }
}

/**
 * @brief Thread barrier that can be controlled from an external thread
 */
template <ThreadType type>
class BarrierWithTrigger
{
public:
    TWINE_DECLARE_NON_COPYABLE(BarrierWithTrigger);
    /**
     * @brief Multithread barrier with trigger functionality
     */
    BarrierWithTrigger()
    {
        mutex_create<type>(&_calling_mutex, nullptr);
        condition_var_create<type>(&_calling_cond, nullptr);
        semaphore_create<type>(&_semaphores[0]);
        semaphore_create<type>(&_semaphores[1]);
        _active_sem = &_semaphores[0];
    }

    /**
     * @brief Destroy the barrier object
     */
    ~BarrierWithTrigger()
    {
        mutex_destroy<type>(&_calling_mutex);
        condition_var_destroy<type>(&_calling_cond);
        semaphore_destroy<type>(&_semaphores[0]);
        semaphore_destroy<type>(&_semaphores[1]);
    }

    /**
     * @brief Wait for signal to finish, called from threads participating on the
     *        barrier
     */
    void wait()
    {
        mutex_lock<type>(&_calling_mutex);
        auto active_sem = _active_sem;
        if (++_no_threads_currently_on_barrier >= _no_threads)
        {
            condition_signal<type>(&_calling_cond);
        }
        mutex_unlock<type>(&_calling_mutex);

        semaphore_wait<type>(active_sem);
    }

    /**
     * @brief Wait for all threads to halt on the barrier, called from a thread
     *        not waiting on the barrier and will block until all threads are
     *        waiting on the barrier.
     */
    void wait_for_all()
    {
        mutex_lock<type>(&_calling_mutex);
        int current_threads = _no_threads_currently_on_barrier;

        if (current_threads == _no_threads)
        {
            mutex_unlock<type>(&_calling_mutex);
            return;
        }
        while (current_threads < _no_threads)
        {
            condition_wait<type>(&_calling_cond, &_calling_mutex);
            current_threads = _no_threads_currently_on_barrier;
        }
        mutex_unlock<type>(&_calling_mutex);
    }

    /**
     * @brief Change the number of threads for the barrier to handle.
     * @param threads
     */
    void set_no_threads(int threads)
    {
        mutex_lock<type>(&_calling_mutex);
        _no_threads = threads;
        mutex_unlock<type>(&_calling_mutex);
    }

    /**
     * @brief Release all threads waiting on the barrier.
     */
    void release_all()
    {
        mutex_lock<type>(&_calling_mutex);

        assert(_no_threads_currently_on_barrier == _no_threads);
        _no_threads_currently_on_barrier = 0;

        auto prev_sem = _active_sem;
        _swap_semaphores();

        for (int i = 0; i < _no_threads; ++i)
        {
            semaphore_signal<type>(prev_sem);
        }

        mutex_unlock<type>(&_calling_mutex);
    }

    void release_and_wait()
    {
        mutex_lock<type>(&_calling_mutex);
        assert(_no_threads_currently_on_barrier == _no_threads);
        _no_threads_currently_on_barrier = 0;

        auto prev_sem = _active_sem;
        _swap_semaphores();

        for (int i = 0; i < _no_threads; ++i)
        {
            semaphore_signal<type>(prev_sem);
        }

        int current_threads = _no_threads_currently_on_barrier;

        while (current_threads < _no_threads)
        {
            condition_wait<type>(&_calling_cond, &_calling_mutex);
            current_threads = _no_threads_currently_on_barrier;
        }
        mutex_unlock<type>(&_calling_mutex);
    }

private:
    void _swap_semaphores()
    {
        if (_active_sem == &_semaphores[0])
        {
            _active_sem = &_semaphores[1];
        }
        else
        {
            _active_sem = &_semaphores[0];
        }
    }

    std::array<sem_t, 2> _semaphores;
    sem_t* _active_sem;

    pthread_mutex_t _calling_mutex;
    pthread_cond_t  _calling_cond;

    std::atomic<int> _no_threads_currently_on_barrier{0};
    std::atomic<int> _no_threads{0};
};

template <ThreadType type>
class WorkerThread
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerThread);

    WorkerThread(BarrierWithTrigger<type>& barrier, WorkerCallback callback,
                                         void*callback_data, std::atomic_bool& running_flag,
                                         bool disable_denormals,
                                         bool break_on_mode_sw): _barrier(barrier),
                                                                  _callback(callback),
                                                                  _callback_data(callback_data),
                                                                  _running(running_flag),
                                                                  _disable_denormals(disable_denormals),
                                                                  _break_on_mode_sw(break_on_mode_sw)

    {}

    ~WorkerThread()
    {
        if (_thread_handle != 0)
        {
            thread_join<type>(_thread_handle, nullptr);
        }
    }

    int run(int cpu_id)
    {
        struct sched_param rt_params = {.sched_priority = 75};
        pthread_attr_t task_attributes;
        pthread_attr_init(&task_attributes);

        pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
        pthread_attr_setschedparam(&task_attributes, &rt_params);
        auto res = 0;
#ifndef __APPLE__
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(cpu_id, &cpus);
        res = pthread_attr_setaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);
#endif
        if (res == 0)
        {
            return thread_create<type>(&_thread_handle, &task_attributes, &_worker_function, this);
        }
        return res;
    }

    static void* _worker_function(void* data)
    {
        reinterpret_cast<WorkerThread<type>*>(data)->_internal_worker_function();
        return nullptr;
    }

private:
    void _internal_worker_function()
    {
        // Signal that this is a realtime thread
        ThreadRtFlag rt_flag;
        if (_disable_denormals)
        {
            set_flush_denormals_to_zero();
        }
        if (type == ThreadType::XENOMAI && _break_on_mode_sw)
        {
            enable_break_on_mode_sw();
        }

        while (true)
        {
            _barrier.wait();
            if (_running.load() == false)
            {
                // condition checked when coming out of wait as we might want to exit immediately here
                break;
            }
            _callback(_callback_data);
        }
    }

    BarrierWithTrigger<type>&   _barrier;
    pthread_t                   _thread_handle{0};
    WorkerCallback              _callback;
    void*                       _callback_data;
    const std::atomic_bool&     _running;
    bool                        _disable_denormals;
    bool                        _break_on_mode_sw;
};

template <ThreadType type>
class WorkerPoolImpl : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerPoolImpl);

    explicit WorkerPoolImpl(int cores,
                            bool disable_denormals,
                            bool break_on_mode_sw) : _no_cores(cores),
                                                     _disable_denormals(disable_denormals),
                                                     _break_on_mode_sw(break_on_mode_sw)
    {}

    ~WorkerPoolImpl()
    {
        _barrier.wait_for_all();
        _running.store(false);
        _barrier.release_all();
    }

    WorkerPoolStatus add_worker(WorkerCallback worker_cb, void* worker_data) override
    {
        auto worker = std::make_unique<WorkerThread<type>>(_barrier, worker_cb, worker_data, _running,
                                                           _disable_denormals, _break_on_mode_sw);
        _barrier.set_no_threads(_no_workers + 1);
        // round-robin assignment to cpu cores
        int core = (_no_workers + 1) % _no_cores;
        auto res = errno_to_worker_status(worker->run(core));
        if (res == WorkerPoolStatus::OK)
        {
            // Wait until the thread is idle to avoid synchronisation issues
            _no_workers++;
            _workers.push_back(std::move(worker));
            _barrier.wait_for_all();
        }
        else
        {
            _barrier.set_no_threads(_no_workers);
        }
        return res;
    }

    void wait_for_workers_idle() override
    {
        _barrier.wait_for_all();
    }

    void wakeup_workers() override
    {
        _barrier.release_all();
    }

    void wakeup_and_wait() override
    {
        _barrier.release_and_wait();
    }

private:
    std::atomic_bool            _running{true};
    int                         _no_workers{0};
    int                         _no_cores;
    bool                        _disable_denormals;
    bool                        _break_on_mode_sw;
    BarrierWithTrigger<type>    _barrier;
    std::vector<std::unique_ptr<WorkerThread<type>>> _workers;
};

}// namespace twine

#endif //TWINE_WORKER_POOL_IMPLEMENTATION_H
