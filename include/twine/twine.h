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
#include <string>
#include <functional>

#ifdef TWINE_APPLE_THREADING
#include <mach/mach_time.h>

#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
#include <os/workgroup.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#endif

namespace twine::apple
{
enum class AppleThreadingStatus
{
    OK = 0,
    WG_CANCELLED = 1,
    WG_FAILED = 2,
    WG_SIZE_FAILED = 3,
    FETCH_NAME_SIZE_FAILED = 4,
    FETCH_NAME_FAILED = 5,
    PD_FAILED = 6,
    PD_SIZE_FAILED = 7,
    MACOS_11_NOT_DETECTED = 8,
    INVALID_DEVICE_NAME_PASSED = 9,

    REALTIME_OK = 10,
    REALTIME_FAILED = 11,
    NO_WORKGROUP_PASSED = 12,
    WORKGROUP_ALREADY_CANCELLED = 13,
    WORKGROUP_JOINING_UNKNOWN_FAILURE = 14,

    EMPTY = 15
};

typedef std::function<void(apple::AppleThreadingStatus)> WorkerErrorCallback;

/**
 * A structure defining what data need to be stored for
 * audio-rate worker threads forming part of the audio workgroup.
 * This data is passed to Worker Pools - If on Apple.
 * If not, it is excluded.
 */
struct AppleMultiThreadData
{
    // The CoreAudio device name, needed for fetching the workgroup ID.
    std::string device_name;

    // These are used by Apple real-time thread groups to calculate the thread periodicity.
    // Make sure you set them to the values used in your audio application.
    double current_sample_rate = 0;
    int chunk_size = 0;
};

}

#else

namespace twine::apple
{

enum class AppleThreadingStatus
{
    OK = 0,
    EMPTY = 19
};

typedef void* AppleMultiThreadData;

typedef std::function<void(apple::AppleThreadingStatus)> WorkerErrorCallback;

}

#endif // __APPLE__

namespace twine {

constexpr int DEFAULT_SCHED_PRIORITY = 75;

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

[[nodiscard]] std::string to_error_string(twine::WorkerPoolStatus status);

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
     * @brief Construct a WorkerPool object. Throws a `std::runtime_error`
     * if construction fails.
     * @param cores The maximum number of cores to use, must not be higher
     *              than the number of cores on the machine.
     * @param apple_data A AppleMultiThreadData struct, with fields set for setting up Apple real-time threads.
     * @param disable_denormals If set, all worker thread sets the FTZ (flush denormals to zero)
     *                          and DAC (denormals are zero) flags.
     * @param break_on_mode_sw If set, enables the break_on_mode_swich flag for every worker
     *                         thread, enabling debugging of memory allocations and syscalls from
     *                         an audio thread. Only enabled for xenomai threads. Argument has no
     *                         effect for posix threads.
     * @return
     */
    [[nodiscard]] static std::unique_ptr<WorkerPool> create_worker_pool(int cores,
                                                                        [[maybe_unused]] apple::AppleMultiThreadData apple_data,
                                                                        bool disable_denormals = true,
                                                                        bool break_on_mode_sw = false);

    virtual ~WorkerPool() = default;

    /**
     * @brief Add a worker to the pool
     * @param worker_cb The worker callback function that will be called by the worker
     * @param worker_data A data pointer that will be passed to the worker callback
     * @param sched_priority Worker priority in [0, 100] (higher numbers mean higher priorities)
     * @param cpu_id Optional CPU core affinity preference. If left unspecified,
     *               the first core with least usage is picked
     *
     * @return WorkerPoolStatus::OK if the operation succeed, error status otherwise
     */
    [[nodiscard]] virtual std::pair<WorkerPoolStatus, apple::AppleThreadingStatus> add_worker(WorkerCallback worker_cb,
                                                                                              void* worker_data,
                                                                                              int sched_priority = DEFAULT_SCHED_PRIORITY,
                                                                                              std::optional<int> cpu_id = std::nullopt) = 0;

    /**
     * @brief Wait for all workers to finish and become idle. Will block until all
     *        workers are idle.
     */
    virtual void wait_for_workers_idle() = 0;

    /**
     * @brief Signal all workers to run call their respective callback functions in
     *        an unspecified order. The call will not block until all workers have finished.
     */
    virtual void wakeup_workers() = 0;

    /**
     * @brief Signal all workers to run call their respective callback functions in
     *        an unspecified order and block until all workers have finished in a
     *        single atomic operation.
     */
    virtual void wakeup_and_wait() = 0;

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
    [[nodiscard]] static std::unique_ptr<RtConditionVariable> create_rt_condition_variable();

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
