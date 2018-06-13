#ifndef TWINE_STD_WORKER_POOL_H
#define TWINE_STD_WORKER_POOL_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <array>

#include "twine.h"
#include "worker_pool_common.h"

namespace twine
{
typedef void (*WorkerCallback)(void* data);

constexpr int MAX_NO_THREADS_ON_BARRIER = 8;

/**
 * @brief Thread barrier that can be controlled from an external thread
 */
class BarrierWithTrigger
{
public:
    /**
     * @brief Multithread barrier with trigger functionality
     * @param threads The default number of threads to handle
     */
    BarrierWithTrigger() = default;

    /**
     * @brief Destroy the barrier object
     */
    ~BarrierWithTrigger();

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
    void relase_all();


private:
    void _swap_halt_flags();

    std::mutex _thread_mutex;
    std::mutex _calling_mutex;

    std::condition_variable _thread_cond;
    std::condition_variable _calling_cond;

    std::array<bool, 2> _halt_flags{true,true};
    bool*           _halt_flag{&_halt_flags[0]};
    std::atomic_int _no_threads_currently_on_barrier{0};
    std::atomic_int _no_threads{0};
};

class StdWorkerThread
{
public:
    StdWorkerThread(BarrierWithTrigger& barrier, WorkerCallback callback,
                    void* callback_data, std::atomic_bool& running_flag);
    ~StdWorkerThread();

private:
    void worker();

    BarrierWithTrigger& _barrier;
    std::thread _thread;
    WorkerCallback _callback;
    void* _callback_data;
    int  _thread_id;
    const std::atomic_bool& _running;
};

class StdWorkerPool : public WorkerPool
{
public:
    StdWorkerPool() = default;
    ~StdWorkerPool() = default;

    int add_worker(WorkerCallback worker_cb, void* worker_data) override;

    void wait_for_workers_idle() override;

    void raspa_wakeup_workers() override;

private:

    int _no_workers{0};

};

}// namespace twine

#endif //TWINE_STD_WORKER_POOL_H
