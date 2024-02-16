/*
 * Copyright Copyright 2017-2023 Elk Audio AB
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
 * @copyright 2017-2023 Elk Audio AB, Stockholm
 */

#ifndef TWINE_WORKER_POOL_IMPLEMENTATION_H
#define TWINE_WORKER_POOL_IMPLEMENTATION_H

#include <cassert>
#include <atomic>
#include <memory>
#include <vector>
#include <array>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <fstream>

#ifdef TWINE_BUILD_WITH_EVL
    #include <unistd.h>
    #include <evl/thread.h>
#endif

#include "apple_threading.h"

#include "twine/twine.h"
#include "thread_helpers.h"
#include "twine_internal.h"

namespace twine {
constexpr auto ISOLATED_CPUS_FILE = "/sys/devices/system/cpu/isolated";

template <ThreadType type>
class WorkerPoolImpl;

void set_flush_denormals_to_zero();

struct CoreInfo
{
    int id;
    int workers;
};

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

inline std::vector<CoreInfo> build_core_list(int start_core, int cores)
{
    std::vector<CoreInfo> list;
    for (int i = start_core; i < cores + start_core; i++)
    {
        list.push_back({i, 0});
    }
    return list;
}

/**
 * @brief Reads the configured isolated cores from a given file
 * @param str a string to  read the configuration from
 * @return A vector of core ids. Empty if the file doesnt exist or there are no isolated cores
 */
inline std::vector<int> read_isolated_cores(const std::string& str)
{
    try
    {
        auto sep = str.find('-');
        if (sep < std::string::npos)
        {
            int first = std::stoi(str.substr(0, sep));
            int last = std::stoi(str.substr(sep + 1));
            std::vector<int> list;
            for (int i = first; i <= last; ++i)
            {
                list.push_back(i);
            }
            return list;
        }
    }
    catch (std::exception& e) {/*pass*/}
    return {};
}

/**
 * @brief Read the isolated cpus from a file path and populates a vector of CoreInfo objects from it
 * @param cpu_file The file to read cpu data from
 * @param cores The maximum number of cores to use. If that is less than the number of isolated cpu cores
 *              only the first cores will be used
 * @return A std::vector<CoreInfo> with the core ids from the file or std::nullopt if the file is empty
 *         or non-existent
 */
inline std::optional<std::vector<CoreInfo>> get_isolated_cpus(const std::string& cpu_file, int cores)
{
    std::fstream file;
    file.open(cpu_file.c_str(), std::ios_base::in);
    if (file.is_open())
    {
        std::string contents;
        std::getline(file, contents);
        auto info = read_isolated_cores(contents);
        if (info.size() > 0)
        {
            std::vector<CoreInfo> list;
            for (int i = 0; i < std::min(static_cast<int>(info.size()), cores); ++i)
            {
                list.push_back({info.at(i), 0});
            }
            return list;
        }
    }
    return std::nullopt;
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
        if constexpr (type == ThreadType::PTHREAD)
        {
            _thread_helper = new PosixThreadHelper();

            _semaphores[0] = new PosixSemaphore();
            _semaphores[1] = new PosixSemaphore();

            _calling_mutex = new PosixMutex();

            _calling_cond = new PosixCondVar();
        }
        else if constexpr (type == ThreadType::COBALT)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
            _thread_helper = new CobaltThreadHelper();

            _semaphores[0] = new CobaltSemaphore();
            _semaphores[1] = new CobaltSemaphore();

            _calling_mutex = new PosixMutex();

            _calling_cond = new PosixCondVar();
#else
            assert(false && "Not built with Cobalt support");
#endif
        }
        else if constexpr (type == ThreadType::EVL)
        {
#ifdef TWINE_BUILD_WITH_EVL
            _thread_helper = new EvlThreadHelper();

            _semaphores[0] = new EvlSemaphore();
            _semaphores[1] = new EvlSemaphore();

            _calling_mutex = new EvlMutex();

            _calling_cond = new EvlCondVar();
#else
            assert(false && "Not built with EVL support");
#endif
        }

        int res = _thread_helper->semaphore_create(_semaphores[0], "/twine-barrier-sem-0");
        if (res != 0)
        {
            throw std::runtime_error(strerror(res));
        }
        res = _thread_helper->semaphore_create(_semaphores[1], "/twine-barrier-sem-1");
        if (res != 0)
        {
            throw std::runtime_error(strerror(res));
        }
        _thread_helper->mutex_create(_calling_mutex, "/twine-barrier-mutex");
        _thread_helper->condition_var_create(_calling_cond, "/twine-barrier-condvar");
    }

    /**
     * @brief Destroy the barrier object
     */
    ~BarrierWithTrigger()
    {
        _thread_helper->mutex_destroy(_calling_mutex);
        _thread_helper->condition_var_destroy(_calling_cond);
        _thread_helper->semaphore_destroy(_semaphores[0], "/twine-barrier-sem-0");
        _thread_helper->semaphore_destroy(_semaphores[1], "/twine-barrier-sem-1");

        delete _semaphores[0];
        delete _semaphores[1];
        delete _calling_mutex;
        delete _calling_cond;
        delete _thread_helper;
    }

    /**
     * @brief Wait for signal to finish, called from threads participating on the
     *        barrier
     */
    void wait()
    {
        _thread_helper->mutex_lock(_calling_mutex);
        auto active_sem = _semaphores[_active_sem_idx];
        if (++_no_threads_currently_on_barrier >= _no_threads)
        {
            _thread_helper->condition_signal(_calling_cond);
        }
        _thread_helper->mutex_unlock(_calling_mutex);

        _thread_helper->semaphore_wait(active_sem);
    }

    /**
     * @brief Wait for all threads to halt on the barrier, called from a thread
     *        not waiting on the barrier and will block until all threads are
     *        waiting on the barrier.
     */
    void wait_for_all()
    {
        _thread_helper->mutex_lock(_calling_mutex);
        int current_threads = _no_threads_currently_on_barrier;

        if (current_threads == _no_threads)
        {
            _thread_helper->mutex_unlock(_calling_mutex);
            return;
        }
        while (current_threads < _no_threads)
        {
            _thread_helper->condition_wait(_calling_cond, _calling_mutex);
            current_threads = _no_threads_currently_on_barrier;
        }
        _thread_helper->mutex_unlock(_calling_mutex);
    }

    /**
     * @brief Change the number of threads for the barrier to handle.
     * @param threads
     */
    void set_no_threads(int threads)
    {
        _thread_helper->mutex_lock(_calling_mutex);
        _no_threads = threads;
        _thread_helper->mutex_unlock(_calling_mutex);
    }

    /**
     * @brief Release all threads waiting on the barrier.
     */
    void release_all()
    {
        _thread_helper->mutex_lock(_calling_mutex);

        assert(_no_threads_currently_on_barrier == _no_threads);
        _no_threads_currently_on_barrier = 0;

        auto prev_sem = _semaphores[_active_sem_idx];
        _swap_semaphores();

        for (int i = 0; i < _no_threads; ++i)
        {
            _thread_helper->semaphore_signal(prev_sem);
        }

        _thread_helper->mutex_unlock(_calling_mutex);
    }

    void release_and_wait()
    {
        _thread_helper->mutex_lock(_calling_mutex);
        assert(_no_threads_currently_on_barrier == _no_threads);
        _no_threads_currently_on_barrier = 0;

        auto prev_sem = _semaphores[_active_sem_idx];
        _swap_semaphores();

        for (int i = 0; i < _no_threads; ++i)
        {
            _thread_helper->semaphore_signal(prev_sem);
        }

        int current_threads = _no_threads_currently_on_barrier;

        while (current_threads < _no_threads)
        {
            _thread_helper->condition_wait(_calling_cond, _calling_mutex);
            current_threads = _no_threads_currently_on_barrier;
        }
        _thread_helper->mutex_unlock(_calling_mutex);
    }

private:
    void _swap_semaphores()
    {
        _active_sem_idx = 1 - _active_sem_idx;
    }

    BaseThreadHelper* _thread_helper;

    std::array<BaseSemaphore*, 2> _semaphores;
    int _active_sem_idx {0};

    BaseMutex* _calling_mutex;
    BaseCondVar* _calling_cond;

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
        if constexpr (type == ThreadType::PTHREAD)
        {
            _thread_helper = new PosixThreadHelper();
        }
        else if constexpr (type == ThreadType::COBALT)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
            _thread_helper = new CobaltThreadHelper();
#else
            assert(false && "Not built with Cobalt support");
#endif
        }
        else if constexpr (type == ThreadType::EVL)
        {
#ifdef TWINE_BUILD_WITH_EVL
            _thread_helper = new EvlThreadHelper();
#else
            assert(false && "Not built with EVL support");
#endif
        }
#if defined(TWINE_APPLE_THREADING) && defined(TWINE_BUILD_WITH_APPLE_COREAUDIO)

        if (__builtin_available(macOS 11.00, *))
        {
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
        }
        else
        {
            _status = apple::AppleThreadingStatus::OK;
            _p_workgroup = nullptr;
        }
#endif
    }

    ~WorkerThread()
    {
        if (_thread_handle != 0)
        {
            _thread_helper->thread_join(_thread_handle, nullptr);
        }

        delete _thread_helper;
    }

    int run(int sched_priority, [[maybe_unused]] int cpu_id)
    {
        if ( (sched_priority < 0) || (sched_priority > 100) )
        {
            return EINVAL;
        }
        _priority = sched_priority;
        // TODO - Why was rt_params moved to only apple on te apple branch?
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
            res = _thread_helper->thread_create(&_thread_handle, &task_attributes, &_worker_function, this);
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
        if (type == ThreadType::COBALT && _break_on_mode_sw)
        {
#ifdef TWINE_BUILD_WITH_XENOMAI
            pthread_setmode_np(0, PTHREAD_WARNSW, 0);
#endif
        }

#ifdef TWINE_BUILD_WITH_EVL
        if constexpr (type == ThreadType::EVL)
        {
            auto tfd = evl_attach_self("/twine-worker-%d", gettid());
            if (_break_on_mode_sw)
            {
                evl_set_thread_mode(tfd, T_WOSS, NULL);
            }
        }
#endif
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
    }
#endif

    friend class WorkerPoolImpl<ThreadType::PTHREAD>;
    friend class WorkerPoolImpl<ThreadType::COBALT>;
    friend class WorkerPoolImpl<ThreadType::EVL>;

    void _stop_thread()
    {
        _thread_running.store(false);

        _barrier.release_all();
    }

    BarrierWithTrigger<type>&   _barrier;
    pthread_t                   _thread_handle{0};
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

    BaseThreadHelper*           _thread_helper;
};

template <ThreadType type>
class WorkerPoolImpl : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(WorkerPoolImpl);

    explicit WorkerPoolImpl(int cores,
                            [[maybe_unused]] apple::AppleMultiThreadData apple_data,
                            bool disable_denormals,
                            bool break_on_mode_sw) : _disable_denormals(disable_denormals),
                                                     _break_on_mode_sw(break_on_mode_sw),
                                                     _apple_data(apple_data)
    {
#ifdef TWINE_BUILD_WITH_EVL
        // EVL supports isolated cpus, if that is enabled we need to assign workers only to those cores
        // If isolated cpus is not enabled we simply start counting from 0
        if constexpr (type == ThreadType::EVL)
        {
            _cores = get_isolated_cpus(ISOLATED_CPUS_FILE, cores).value_or(build_core_list(0, cores));
        }
        else
        {
            _cores = build_core_list(0, cores);
        }
#else
        _cores = build_core_list(0, cores);

#endif

    }

    ~WorkerPoolImpl() override
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
        std::vector<CoreInfo>::iterator core_info;
        if (cpu_id.has_value())
        {
            auto core = std::find_if(_cores.begin(), _cores.end(), [&](auto& i){return i.id == cpu_id.value();});
            if (core == _cores.end())
            {
                return {WorkerPoolStatus::INVALID_ARGUMENTS, apple::AppleThreadingStatus::EMPTY};
            }
            core_info = core;
        }
        else
        {
            // If no core is specified, pick the first core with the least usage
            auto core = std::min_element(_cores.begin(), _cores.end(),
                                         [](auto& lhs, auto& rhs){return lhs.workers < rhs.workers;});
            core_info = core;
        }

        auto worker = std::make_unique<WorkerThread<type>>(_barrier,
                                                           worker_cb,
                                                           worker_data,
                                                           _apple_data,
                                                           _running,
                                                           _disable_denormals,
                                                           _break_on_mode_sw);
        _barrier.set_no_threads(_no_workers + 1);

        core_info->workers++;

        auto res = errno_to_worker_status(worker->run(sched_priority, core_info->id));
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
                core_info->workers--;

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
    std::vector<CoreInfo>       _cores;
    bool                        _disable_denormals;
    bool                        _break_on_mode_sw;
    BarrierWithTrigger<type>    _barrier;
    std::vector<std::unique_ptr<WorkerThread<type>>> _workers;

    apple::AppleMultiThreadData _apple_data;
};

}// namespace twine

#endif //TWINE_WORKER_POOL_IMPLEMENTATION_H
