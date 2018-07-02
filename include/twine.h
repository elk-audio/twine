#ifndef TWINE_TWINE_H_
#define TWINE_TWINE_H_

#include <memory>

namespace twine {

/**
 * @brief Function to determine the realtime processing state of the current thread
 * @return true if called from a realtime audio processing thread, false other
 */
bool is_current_thread_realtime();

typedef void (*WorkerCallback)(void* data);

enum class WorkerPoolStatus
{
    OK,
    ERROR,
    PERMISSION_DENIED,
    LIMIT_EXCEEDED
};

class WorkerPool
{
public:
    /**
     * @brief Construct a WorkerPool object.
     * @param cores The maximum number of cores to use, must not be higher
     *              than the number of cores on the machine.
     * @return
     */
    static std::unique_ptr<WorkerPool> create_worker_pool(int cores);

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