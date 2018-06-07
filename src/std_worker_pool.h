#ifndef TWINE_STD_WORKER_POOL_H
#define TWINE_STD_WORKER_POOL_H

#include "twine.h"

namespace twine
{
typedef void (*WorkerCallback)(void* data);

class StdWorkerPool : public WorkerPool
{
public:
    ~StdWorkerPool() = default;

    int add_worker(WorkerCallback worker_cb, void* worker_data) override {} ;

    void wait_for_workers_idle() override {};

    void raspa_wakeup_workers() override {};

};

}// namespace twine

#endif //TWINE_STD_WORKER_POOL_H
