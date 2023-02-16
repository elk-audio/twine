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
#include <stdexcept>
#include <string>

#include "apple_threading.h"

#include "thread_helpers.h"
#include "twine_internal.h"

namespace twine {

template <ThreadType type>
class WorkerPoolImpl;

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

        case EINVAL:
            return WorkerPoolStatus::INVALID_ARGUMENTS;

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
     * @brief Multi-thread barrier with trigger functionality
     */
    BarrierWithTrigger()
    {
        if constexpr (type == ThreadType::XENOMAI)
        {
            _semaphores[0] = &_semaphore_store[0];
            _semaphores[1] = &_semaphore_store[1];
        }
        mutex_create<type>(&_calling_mutex, nullptr);
        condition_var_create<type>(&_calling_cond, nullptr);
        int res = semaphore_create<type>(&_semaphores[0], "twine_semaphore_0");
        if (res != 0)
        {
            throw std::runtime_error(strerror(res));
        }
        res = semaphore_create<type>(&_semaphores[1], "twine_semaphore_1");
        if (res != 0)
        {
            throw std::runtime_error(strerror(res));
        }
        _active_sem = _semaphores[0];
    }

    /**
     * @brief Destroy the barrier object
     */
    ~BarrierWithTrigger()
    {
        mutex_destroy<type>(&_calling_mutex);
        condition_var_destroy<type>(&_calling_cond);
        semaphore_destroy<type>(_semaphores[0], "twine_semaphore_0");
        semaphore_destroy<type>(_semaphores[1], "twine_semaphore_1");
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
        if (_active_sem == _semaphores[0])
        {
            _active_sem = _semaphores[1];
        }
        else
        {
            _active_sem = _semaphores[0];
        }
    }

    std::array<sem_t, 2 > _semaphore_store;
    std::array<sem_t*, 2> _semaphores;
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

    WorkerThread(BarrierWithTrigger<type>& barrier,
                 WorkerCallback callback,
                 void* callback_data,
                 apple::AppleMultiThreadData& apple_data,
                 std::atomic_bool& running_flag,
                 bool disable_denormals,
                 bool break_on_mode_sw): _barrier(barrier),
                                         _callback(callback),
                                         _callback_data(callback_data),
                                         _apple_data(apple_data),
                                         _pool_running(running_flag),
                                         _disable_denormals(disable_denormals),
                                         _break_on_mode_sw(break_on_mode_sw)

    {
#if defined(TWINE_APPLE_THREADING) && defined(TWINE_BUILD_WITH_APPLE_COREAUDIO)
        // The workgroup will be the same for all threads, so it only needs to be fetched once.
        auto device_workgroup_result = twine::apple::get_device_workgroup(_apple_data.device_name);

        if (device_workgroup_result.second != twine::apple::AppleThreadingStatus::OK)
        {
            _status = device_workgroup_result.second;
        }
        else
        {
            _status = apple::AppleThreadingStatus::OK;
        }

        _p_workgroup = device_workgroup_result.first;
#endif
    }

    ~WorkerThread()
    {
        if (_thread_handle != 0)
        {
            thread_join<type>(_thread_handle, nullptr);
        }
    }

    int run(int sched_priority, [[maybe_unused]] int cpu_id)
    {
        if ( (sched_priority < 0) || (sched_priority > 100) )
        {
            return EINVAL;
        }
        _priority = sched_priority;

        pthread_attr_t task_attributes;
        pthread_attr_init(&task_attributes);

        pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);

        auto res = 0;

#ifndef __APPLE__
        struct sched_param rt_params = {.sched_priority = sched_priority};
        pthread_attr_setschedparam(&task_attributes, &rt_params);

        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(cpu_id, &cpus);
        res = pthread_attr_setaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);
#endif

        if (res == 0)
        {
            res = thread_create<type>(&_thread_handle, &task_attributes, &_worker_function, this);
        }
        pthread_attr_destroy(&task_attributes);
        return res;
    }

    static void* _worker_function(void* data)
    {
        reinterpret_cast<WorkerThread<type>*>(data)->_internal_worker_function();
        return nullptr;
    }

    apple::AppleThreadingStatus init_status()
    {
        return _status;
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

#ifdef TWINE_APPLE_THREADING
        _init_apple_thread();
#endif

        while (true)
        {
            _barrier.wait();
            if (_pool_running.load() == false || _thread_running.load() == false)
            {
                // condition checked when coming out of wait as we might want to exit immediately here
                break;
            }
            _callback(_callback_data);
        }

#if defined(TWINE_APPLE_THREADING) && defined(TWINE_BUILD_WITH_APPLE_COREAUDIO)
        apple::leave_workgroup_if_needed(&_join_token, _p_workgroup);
#endif
    }

#if defined(TWINE_APPLE_THREADING)
    void _init_apple_thread()
    {
        assert(_apple_data.chunk_size != 0);
        assert(_apple_data.current_sample_rate != 0);

        double period_ms = std::max(1000.0 * _apple_data.chunk_size / _apple_data.current_sample_rate,
                                    1.0);

        if (apple::set_current_thread_to_realtime(period_ms) == false)
        {
            _status = apple::AppleThreadingStatus::REALTIME_FAILED;
        }

        if (__builtin_available(macOS 11.00, *))
        {
            apple::AppleThreadingStatus status;

            std::tie(status, _join_token) = apple::join_workgroup(_p_workgroup);
            if (status != apple::AppleThreadingStatus::OK)
            {
                _status = status;
            }
        }
        else if (__builtin_available(macOS 10.10, *))
        {
            // IF this is used on macOS that is not 11: try to set QoS.
            int error = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
            switch (error)
            {
                case 0: // Successful
                {
                    // No need to set to OK - it's the default value.
                    break;
                }
                case EAGAIN:
                {
                    _status = apple::AppleThreadingStatus::QOS_EAGAIN;
                    break;
                }
                case EPERM:
                {
                    _status = apple::AppleThreadingStatus::QOS_EPERM;
                    break;
                }
                case EINVAL:
                {
                    _status = apple::AppleThreadingStatus::QOS_EINVAL;
                    break;
                }
                default:
                {
                    _status = apple::AppleThreadingStatus::QOS_UNKNOWN;
                    break;
                }
            }
        }
    }
#endif

    friend class WorkerPoolImpl<ThreadType::PTHREAD>;
    friend class WorkerPoolImpl<ThreadType::XENOMAI>;

    void _stop_thread()
    {
        _thread_running.store(false);

        _barrier.release_all();
    }

    BarrierWithTrigger<type>&   _barrier;
    pthread_t                   _thread_handle {0};
    WorkerCallback              _callback;
    void*                       _callback_data;

    apple::AppleMultiThreadData& _apple_data;

#ifdef TWINE_APPLE_THREADING
    os_workgroup_join_token_s   _join_token;
    os_workgroup_t              _p_workgroup;
#endif

    std::atomic<apple::AppleThreadingStatus> _status {apple::AppleThreadingStatus::OK};

    const std::atomic_bool&     _pool_running;
    std::atomic_bool            _thread_running {true};

    bool                        _disable_denormals;
    int                         _priority {0};
    bool                        _break_on_mode_sw;
};

template <ThreadType type>
class WorkerPoolImpl : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerPoolImpl);

    explicit WorkerPoolImpl(int cores,
                            [[maybe_unused]] apple::AppleMultiThreadData apple_data,
                            bool disable_denormals,
                            bool break_on_mode_sw) : _no_cores(cores),
                                                     _cores_usage(cores, 0),
                                                     _disable_denormals(disable_denormals),
                                                     _break_on_mode_sw(break_on_mode_sw),
                                                     _apple_data(apple_data)
    {}

    ~WorkerPoolImpl()
    {
        _barrier.wait_for_all();
        _running.store(false);
        _barrier.release_all();
    }

    std::pair<WorkerPoolStatus, apple::AppleThreadingStatus> add_worker(WorkerCallback worker_cb,
                                                                        void* worker_data,
                                                                        int sched_priority = DEFAULT_SCHED_PRIORITY,
                                                                        std::optional<int> cpu_id = std::nullopt) override
    {
        int core = 0;
        if (cpu_id.has_value())
        {
            core = cpu_id.value();
            if ( (core < 0) || (core >= _no_cores) )
            {
                return {WorkerPoolStatus::INVALID_ARGUMENTS, apple::AppleThreadingStatus::EMPTY};
            }
        }
        else
        {
            // If no core is specified, pick the first core with the least usage
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

        auto worker = std::make_unique<WorkerThread<type>>(_barrier,
                                                            worker_cb,
                                                            worker_data,
                                                            _apple_data,
                                                            _running,
                                                           _disable_denormals,
                                                           _break_on_mode_sw);
        _barrier.set_no_threads(_no_workers + 1);

        _cores_usage[core]++;

        auto res = errno_to_worker_status(worker->run(sched_priority, core));

        if (res == WorkerPoolStatus::OK)
        {
            // Wait until the thread is idle to avoid synchronisation issues
            _no_workers++;
            _workers.push_back(std::move(worker));
            _barrier.wait_for_all();

            // Currently, potential failures in worker threads happen only during initialisation.
            // If that changes in the future, checking the status only on start will not suffice.
            auto& w = _workers.back();
            if (w->init_status() != apple::AppleThreadingStatus::OK)
            {
                auto status = w->init_status();

                // On failure, the thread is removed and discarded.
                // The Twine host needs to decide if this is considered recoverable, or if it should exit.
                w->_stop_thread();

                _no_workers--;
                _barrier.set_no_threads(_no_workers);
                _cores_usage[core]--;

                _workers.pop_back();

                return {WorkerPoolStatus::ERROR, status};
            }
        }
        else
        {
            _barrier.set_no_threads(_no_workers);
        }

        return {res, apple::AppleThreadingStatus::OK};
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
    std::vector<int>            _cores_usage;
    bool                        _disable_denormals;
    bool                        _break_on_mode_sw;
    BarrierWithTrigger<type>    _barrier;
    std::vector<std::unique_ptr<WorkerThread<type>>> _workers;

    apple::AppleMultiThreadData _apple_data;
};

}// namespace twine

#endif //TWINE_WORKER_POOL_IMPLEMENTATION_H
