#include "twine.h"
#include "twine_internal.h"
#include "worker_pool_implementation.h"

namespace twine {

thread_local int ThreadRtFlag::_instance_counter = 0;
bool XenomaiRtFlag::_enabled = false;
static XenomaiRtFlag running_xenomai_realtime;

bool is_current_thread_realtime()
{
    return ThreadRtFlag::is_realtime();
}

void init_xenomai()
{
    running_xenomai_realtime.set(true);
}

std::unique_ptr<WorkerPool> WorkerPool::CreateWorkerPool(int cores)
{
    if (running_xenomai_realtime.is_set())
    {
        return std::make_unique<WorkerPoolImpl<ThreadType::XENOMAI>>(cores);
    }
    return std::make_unique<WorkerPoolImpl<ThreadType::PTHREAD>>(cores);
}

} // twine