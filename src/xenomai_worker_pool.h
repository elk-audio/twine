#ifndef TWINE_XENOMAI_WORKER_POOL_H
#define TWINE_XENOMAI_WORKER_POOL_H

#include "worker_pool.h"

namespace twine
{
typedef void (*RaspaWorkerCallback)(void* data);

class XenomaiWorkerPool : public WorkerPool
{
public:

    int add_worker(RaspaWorkerCallback worker_cb, void* worker_data) override ;

    void wait_for_workers_idle() override ;

    void raspa_wakeup_workers() override ;

};

}// namespace twine

#endif // TWINE_XENOMAI_WORKER_POOL_H