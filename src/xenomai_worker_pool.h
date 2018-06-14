#ifndef TWINE_XENOMAI_WORKER_POOL_H
#define TWINE_XENOMAI_WORKER_POOL_H

#include "twine.h"
#include "twine_internal.h"
#include "worker_pool_common.h"

namespace twine {

class XenomaiWorkerBarrier
{
public:
    XenomaiWorkerBarrier();

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int n_threads_on_barrier;
};

struct XenomaiBarriers
{
    XenomaiWorkerBarrier currently_idle;
    XenomaiWorkerBarrier work_ready;
    XenomaiWorkerBarrier currently_working;
    XenomaiWorkerBarrier can_finish;
};

class XenomaiWorkerThread
{
public:
    TWINE_DECLARE_NON_COPYABLE(XenomaiWorkerThread);

    XenomaiWorkerThread();
    XenomaiBarriers* barriers;
    pthread_t aux_thread;
    WorkerCallback callback;
    void* callback_data;
    int thread_id;
};

class XenomaiWorkerPool : public WorkerPool
{
public:
    TWINE_DECLARE_NON_COPYABLE(XenomaiWorkerPool);

    XenomaiWorkerPool(int cores);
    ~XenomaiWorkerPool();

    int add_worker(WorkerCallback worker_cb, void* worker_data) override ;

    void wait_for_workers_idle() override ;

    void wakeup_workers() override ;


private:
    XenomaiBarriers     _barriers;
    XenomaiWorkerThread _worker_threads[MAX_WORKERS_PER_POOL];
    int                 _n_workers{0};
    int                 _n_cores;
};

}// namespace twine

#endif // TWINE_XENOMAI_WORKER_POOL_H