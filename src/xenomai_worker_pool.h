#ifndef TWINE_XENOMAI_WORKER_POOL_H
#define TWINE_XENOMAI_WORKER_POOL_H

#include "twine.h"

namespace twine {

constexpr int MAX_WORKERS_PER_POOL = 8;
constexpr int N_CPU_CORES = 4;

class WorkerBarrier
{
public:
    WorkerBarrier();

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int n_threads_on_barrier;
};

struct Barriers
{
    WorkerBarrier currently_idle;
    WorkerBarrier work_ready;
    WorkerBarrier currently_working;
    WorkerBarrier can_finish;
};

class WorkerThread
{
public:
    WorkerThread();
    Barriers* barriers;
    pthread_t aux_thread;
    WorkerCallback callback;
    void* callback_data;
    int thread_id;
};

class XenomaiWorkerPool : public WorkerPool
{
public:
    XenomaiWorkerPool();
    ~XenomaiWorkerPool();

    int add_worker(WorkerCallback worker_cb, void* worker_data) override ;

    void wait_for_workers_idle() override ;

    void raspa_wakeup_workers() override ;


private:
    Barriers     _barriers;
    WorkerThread _worker_threads[MAX_WORKERS_PER_POOL];
    int          _n_workers{0};
};

}// namespace twine

#endif // TWINE_XENOMAI_WORKER_POOL_H