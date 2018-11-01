#ifndef TWINE_TWINE_H_
#define TWINE_TWINE_H_

#include <memory>
#include <chrono>

namespace twine {

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

typedef void (*WorkerCallback)(void* data);

enum class WorkerPoolStatus
{
    OK,
    ERROR,
    PERMISSION_DENIED,
    LIMIT_EXCEEDED
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
     * @return WorkerPoolStatus::OK if the operation succeed, error status otherwise
     */
    virtual WorkerPoolStatus add_worker(WorkerCallback worker_cb, void* worker_data) = 0;

    /**
     * @brief Wait for all workers to finish and become idle. Will block until all
     *        workers are idle.
     */
    virtual void wait_for_workers_idle() = 0;

    /**
     * @brief After calling, all workers will be signaled to run and will call their
     *        respective callback functions in a unspecified order. The call will block
     *        until all workers have finished and returned to idle.
     */
    virtual void wakeup_workers() = 0;

protected:
    WorkerPool() = default;
};


}// namespace twine

#endif // TWINE_TWINE_H_