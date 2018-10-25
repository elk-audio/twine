#include "twine/twine.h"
#include "twine_internal.h"
#include "worker_pool_implementation.h"

namespace twine {

constexpr int64_t NS_TO_S = 1'000'000'000;

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

std::unique_ptr<WorkerPool> WorkerPool::create_worker_pool(int cores, bool disable_denormals)
{
    if (running_xenomai_realtime.is_set())
    {
        return std::make_unique<WorkerPoolImpl<ThreadType::XENOMAI>>(cores, disable_denormals);
    }
    return std::make_unique<WorkerPoolImpl<ThreadType::PTHREAD>>(cores, disable_denormals);
}

std::chrono::nanoseconds current_rt_time()
{
    if (running_xenomai_realtime.is_set())
    {
        timespec tp;
        __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);
        return std::chrono::nanoseconds(tp.tv_nsec + tp.tv_sec * NS_TO_S);
    }
    else
    {
        return std::chrono::high_resolution_clock::now().time_since_epoch();
    }
}


} // twine