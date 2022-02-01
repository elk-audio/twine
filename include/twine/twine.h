/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
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

#ifndef TWINE_TWINE_H_
#define TWINE_TWINE_H_

#include <memory>
#include <chrono>
#include <optional>

namespace twine {

struct VersionInfo
{
    int major;
    int minor;
    int revision;
};

/**
 * @brief Query the current version of the library
 * @return A VersionInfo struct with the major, minor and rev number of the library
 */
VersionInfo twine_version();

/**
 * @brief Query the build date and commit info
 * @return A string containing build time, date and git commit number.
 */
const char* build_info();

/**
 * @brief Function to determine the realtime processing state of the current thread
 * @return true if called from a realtime audio processing thread, false other
 */
bool is_current_thread_realtime();

/**
 * @brief Sets the FTZ (flush denormals to zero) and DAC (denormals are zero) flags
 *        in the cpu to avoid performance hits of denormals in the audio thread
 *        Only implemented for x86 cpus with SSE support
 */
void set_flush_denormals_to_zero();

int rt_printf(const char *format, ...);

typedef void (*WorkerCallback)(void* data);

enum class WorkerPoolStatus
{
    OK,
    ERROR,
    PERMISSION_DENIED,
    LIMIT_EXCEEDED,
    INVALID_ARGUMENTS
};

/**
 * @brief Returns the current time at the time of the call. This function is safe to call
 *        from an rt context. The time returned should not be used for synchronising audio
 *        events such as note on/offs as this does not represent the time when the audio
 *        buffer will be sent to an output.
 * @return The current time in nanoseconds.
 */
std::chrono::nanoseconds current_rt_time();

class WorkerPool
{
public:
    /**
     * @brief Construct a WorkerPool object.
     * @param cores The maximum number of cores to use, must not be higher
     *              than the number of cores on the machine.
     * @param disable_denormals If set, all worker thread sets the FTZ (flush denormals to zero)
     *                          and DAC (denormals are zero) flags.
     * @return
     */
    static std::unique_ptr<WorkerPool> create_worker_pool(int cores, bool disable_denormals = true);

    virtual ~WorkerPool() = default;

    /**
     * @brief Add a worker to the pool
     * @param worker_cb The worker callback function that will called by he worker
     * @param worker_data A data pointer that will be passed to the worker callback
     * @param sched_priority Worker priority in [0, 100] (higher numbers mean higher priorities)
     * @param cpu_id Optional CPU core affinity preference. If left unspecified,
     *               the first core with least usage is picked
     *
     * @return WorkerPoolStatus::OK if the operation succeed, error status otherwise
     */
    virtual WorkerPoolStatus add_worker(WorkerCallback worker_cb, void* worker_data,
                                        int sched_priority=75,
                                        std::optional<int> cpu_id=std::nullopt) = 0;

    /**
     * @brief Wait for all workers to finish and become idle. Will block until all
     *        workers are idle.
     */
    virtual void wait_for_workers_idle() = 0;

    /**
     * @brief After calling, all workers will be signaled to run and will call their
     *        respective callback functions in an unspecified order. The call will block
     *        until all workers have finished and returned to idle.
     */
    virtual void wakeup_workers() = 0;

protected:
    WorkerPool() = default;
};

/**
 * @brief Condition variable designed to signal a lower priority non-realtime thread
 *        from a realtime thread without causing mode switches or interfering with
 *        the rt thread operation.
 */
class RtConditionVariable
{
public:
    /**
     * @brief Construct an RtConditionVariable object.
     *        Will throw std::runtime_error if xenomai xddp queues are not enabled in the
     *        kernel or the maximum number of instances have been reached.
     * @return
     */
    static std::unique_ptr<RtConditionVariable> create_rt_condition_variable();

    virtual ~RtConditionVariable() = default;

    /**
     * @brief Call from a realtime thread to notify a non-rt thread to wake up.
     */
    virtual void notify() = 0;

    /**
     * @brief Blocks until notify() is called from a realtime thread. Call from a non-rt
     *        thread to wait until the rt thread signals. A maximum of one thread can wait
     *        on the condition variable at a time.
     * @return true if the condition variable was woken up by a call to notify()
     *              spurious wakeups could happen on some systems.
     */
    virtual bool wait() = 0;

protected:
    RtConditionVariable() = default;
};

}// namespace twine

#endif // TWINE_TWINE_H_
