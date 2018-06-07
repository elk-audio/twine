#ifndef TWINE_WORKER_POOL_H_
#define TWINE_WORKER_POOL_H_

#include <memory>

namespace twine
{
    typedef void (*WorkerCallback)(void* data);

class WorkerPool
{
public:
    static std::unique_ptr<WorkerPool> CreateWorkerPool();

    virtual ~WorkerPool() = default;

    virtual int add_worker(WorkerCallback worker_cb, void* worker_data);

    virtual void wait_for_workers_idle();

    virtual void raspa_wakeup_workers();

protected:
    WorkerPool();
};

}// namespace twine

#endif //TWINE_WORKER_POOL_H_
