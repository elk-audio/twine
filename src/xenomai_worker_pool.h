#ifndef TWINE_XENOMAI_WORKER_POOL_H
#define TWINE_XENOMAI_WORKER_POOL_H

#include <atomic>
#include <vector>

#include "twine.h"
#include "twine_internal.h"
#include "worker_pool_common.h"

namespace twine {

/**
 * @brief Thread barrier that can be controlled from an external thread
 */
class XenomaiBarrierWithTrigger
{
public:
    /**
     * @brief Multithread barrier with trigger functionality
     * @param threads The default number of threads to handle
     */
    XenomaiBarrierWithTrigger();

    /**
     * @brief Destroy the barrier object
     */
    ~XenomaiBarrierWithTrigger();

    /**
     * @brief Wait for signal to finish, called from threads participating on the
     *        barrier
     */
    void wait();

    /**
     * @brief Wait for all threads to halt on the barrier, called from a thread
     *        not waiting on the barrier and will block until all threads are
     *        waiting on the barrier.
     */
    void wait_for_all();

    /**
     * @brief Change the number of threads for the barrier to handle.
     * @param threads
     */
    void set_no_threads(int threads);

    /**
     * @brief Release all threads waiting on the barrier.
     */
    void release_all();


private:
    void _swap_halt_flags();

    pthread_mutex_t _thread_mutex;
    pthread_mutex_t _calling_mutex;

    pthread_cond_t _thread_cond;
    pthread_cond_t _calling_cond;

    std::array<bool, 2> _halt_flags{true,true};
    bool*           _halt_flag{&_halt_flags[0]};
    int _no_threads_currently_on_barrier{0};
    int _no_threads{0};
};


class XenomaiWorkerThread
{
public:
    TWINE_DECLARE_NON_COPYABLE(XenomaiWorkerThread);

    XenomaiWorkerThread(XenomaiBarrierWithTrigger& barrier, WorkerCallback callback,
                    void* callback_data, std::atomic_bool& running_flag, int id);
    ~XenomaiWorkerThread();

    static void* _worker_function(void* data);

private:
    void _internal_worker_function();

    XenomaiBarrierWithTrigger& _barrier;
    unsigned long int _thread_handle{0};
    WorkerCallback _callback;
    void* _callback_data;
    const std::atomic_bool& _running;
};

class XenomaiWorkerPool : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(XenomaiWorkerPool);

    explicit XenomaiWorkerPool(int cores) : _no_cores(cores) {}
    ~XenomaiWorkerPool();

    int add_worker(WorkerCallback worker_cb, void* worker_data) override;

    void wait_for_workers_idle() override;

    void wakeup_workers() override;

private:
    std::atomic_bool _running{true};
    int _no_workers{0};
    int _no_cores;
    XenomaiBarrierWithTrigger _barrier;
    std::vector<std::unique_ptr<XenomaiWorkerThread>> _workers;
};

}// namespace twine

#endif // TWINE_XENOMAI_WORKER_POOL_H