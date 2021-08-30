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
        mutex_create<type>(&_thread_mutex, nullptr);
        mutex_create<type>(&_calling_mutex, nullptr);
        condition_var_create<type>(&_thread_cond, nullptr);
        condition_var_create<type>(&_calling_cond, nullptr);
    }

    /**
     * @brief Destroy the barrier object
     */
    ~BarrierWithTrigger()
    {
        mutex_destroy<type>(&_thread_mutex);
        mutex_destroy<type>(&_calling_mutex);
        condition_var_destroy<type>(&_thread_cond);
        condition_var_destroy<type>(&_calling_cond);
    }

    /**
     * @brief Wait for signal to finish, called from threads participating on the
     *        barrier
     */
    void wait()
    {
        const bool& halt_flag = *_halt_flag; // 'local' halt flag for this round
        mutex_lock<type>(&_calling_mutex);
        if (++_no_threads_currently_on_barrier >= _no_threads)
        {
            condition_signal<type>(&_calling_cond);
        }
        mutex_unlock<type>(&_calling_mutex);

        mutex_lock<type>(&_thread_mutex);
        while (halt_flag)
        {
            // The condition needs to be rechecked when waking as threads may wake up spuriously
            condition_wait<type>(&_thread_cond, &_thread_mutex);
        }
        mutex_unlock<type>(&_thread_mutex);
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
        _swap_halt_flags();
        _no_threads_currently_on_barrier = 0;
        /* For xenomai threads, it is neccesary to hold the mutex while
         * sending the broadcast. Otherwise deadlocks can occur. For
         * pthreads it is not neccesary but it is recommended for
         * good realtime performance. And surprisingly enough seems
         * a bit faster that without holding the mutex.  */
        mutex_lock<type>(&_thread_mutex);
        condition_broadcast<type>(&_thread_cond);
        mutex_unlock<type>(&_thread_mutex);
        mutex_unlock<type>(&_calling_mutex);
    }


private:
    void _swap_halt_flags()
    {
        *_halt_flag = false;
        if (_halt_flag == &_halt_flags[0])
        {
            _halt_flag = &_halt_flag[1];
        }
        else
        {
            _halt_flag = &_halt_flags[0];
        }
        *_halt_flag = true;
    }

    pthread_mutex_t _thread_mutex;
    pthread_mutex_t _calling_mutex;

    pthread_cond_t _thread_cond;
    pthread_cond_t _calling_cond;

    std::array<bool, 2> _halt_flags{true, true};
    bool*_halt_flag{&_halt_flags[0]};
    int _no_threads_currently_on_barrier{0};
    int _no_threads{0};
};

template <ThreadType type>
class WorkerThread
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerThread);

    WorkerThread(BarrierWithTrigger<type>& barrier, WorkerCallback callback,
                                         void*callback_data, std::atomic_bool& running_flag,
                                         bool disable_denormals): _barrier(barrier),
                                                                  _callback(callback),
                                                                  _callback_data(callback_data),
                                                                  _running(running_flag),
                                                                  _disable_denormals(disable_denormals)

    {}

    ~WorkerThread()
    {
        if (_thread_handle != 0)
        {
            thread_join<type>(_thread_handle, nullptr);
        }
    }

    int run(int sched_priority, int cpu_id)
    {
        struct sched_param rt_params = {.sched_priority = sched_priority};
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
        // this is a realtime thread
        ThreadRtFlag rt_flag;
        if (_disable_denormals)
        {
            set_flush_denormals_to_zero();
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
};

template <ThreadType type>
class WorkerPoolImpl : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerPoolImpl);

    explicit WorkerPoolImpl(int cores, bool disable_denormals) : _no_cores(cores),
                                                                 _cores_usage(cores, 0),
                                                                 _disable_denormals(disable_denormals)
    {}

    ~WorkerPoolImpl()
    {
        _barrier.wait_for_all();
        _running.store(false);
        _barrier.release_all();
    }

    WorkerPoolStatus add_worker(WorkerCallback worker_cb, void* worker_data,
                                int sched_priority=75,
                                std::optional<int> cpu_id=std::nullopt) override
    {
        auto worker = std::make_unique<WorkerThread<type>>(_barrier, worker_cb, worker_data, _running, _disable_denormals);
        _barrier.set_no_threads(_no_workers + 1);

        int core = 0;
        if (cpu_id.has_value())
        {
            core = cpu_id.value();
        }
        else
        {
            // If no core is specified, pick the first core with least usage
            int min_idx = _no_cores - 1;
            int min_usage = _cores_usage[min_idx];
            for (int n = _no_cores-1; n >= 0; n--)
            {
                int cur_usage = _cores_usage[n];
                if (cur_usage <= min_usage)
                {
                    min_usage = cur_usage;
                    min_idx = n;
                }
            }
            core = min_idx;
        }
        _cores_usage[core]++;

        auto res = errno_to_worker_status(worker->run(sched_priority, core));
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

private:
    std::atomic_bool            _running{true};
    int                         _no_workers{0};
    int                         _no_cores;
    std::vector<int>            _cores_usage;
    bool                        _disable_denormals;
    BarrierWithTrigger<type>    _barrier;
    std::vector<std::unique_ptr<WorkerThread<type>>> _workers;
};

}// namespace twine

#endif //TWINE_WORKER_POOL_IMPLEMENTATION_H
