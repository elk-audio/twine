#include <cstdlib>
#include <cassert>

#ifdef TWINE_BUILD_WITH_XENOMAI

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cobalt/pthread.h>
#pragma GCC diagnostic pop

#include "twine.h"
#include "twine_internal.h"
#include "xenomai_worker_pool.h"

namespace twine {


XenomaiBarrierWithTrigger::XenomaiBarrierWithTrigger()
{
    __cobalt_pthread_mutex_init(&_thread_mutex, nullptr);
    __cobalt_pthread_mutex_init(&_calling_mutex, nullptr);
    __cobalt_pthread_cond_init(&_thread_cond, nullptr);
    __cobalt_pthread_cond_init(&_calling_cond, nullptr);
}

XenomaiBarrierWithTrigger::~XenomaiBarrierWithTrigger()
{
    __cobalt_pthread_mutex_destroy(&_thread_mutex);
    __cobalt_pthread_mutex_destroy(&_calling_mutex);
    __cobalt_pthread_cond_destroy(&_thread_cond);
    __cobalt_pthread_cond_destroy(&_calling_cond);
}

void XenomaiBarrierWithTrigger::wait()
{
    const bool& halt_flag = *_halt_flag; // 'local' halt flag for this round
    __cobalt_pthread_mutex_lock(&_calling_mutex);
    _no_threads_currently_on_barrier++;
    bool notify = _no_threads_currently_on_barrier == _no_threads;

    __cobalt_pthread_mutex_unlock(&_calling_mutex);
    if (notify)
    {
        __cobalt_pthread_cond_signal(&_calling_cond);
    }

    __cobalt_pthread_mutex_lock(&_thread_mutex);
    while (halt_flag)
    {
        // Threads may be woken up spuriously, therefore the condition
        // needs to be rechecked when waking up
        __cobalt_pthread_cond_wait(&_thread_cond, &_thread_mutex);
    }
    __cobalt_pthread_mutex_unlock(&_thread_mutex);
}

void XenomaiBarrierWithTrigger::wait_for_all()
{
    __cobalt_pthread_mutex_lock(&_calling_mutex);
    int current_threads = _no_threads_currently_on_barrier;
    if (current_threads == _no_threads)
    {
        return;
    }
    while (current_threads < _no_threads)
    {
        __cobalt_pthread_cond_wait(&_calling_cond, &_calling_mutex);
        current_threads = _no_threads_currently_on_barrier;
    }
    __cobalt_pthread_mutex_unlock(&_calling_mutex);
}

void XenomaiBarrierWithTrigger::set_no_threads(int threads)
{
    _no_threads = threads;
}

void XenomaiBarrierWithTrigger::release_all()
{
    assert(_no_threads_currently_on_barrier == _no_threads);
    __cobalt_pthread_mutex_lock(&_thread_mutex);
    _swap_halt_flags();
    _no_threads_currently_on_barrier = 0;
    __cobalt_pthread_mutex_unlock(&_thread_mutex);
    __cobalt_pthread_cond_broadcast(&_thread_cond);
}

void XenomaiBarrierWithTrigger::_swap_halt_flags()
{
    *_halt_flag = false;
    if (_halt_flag == &_halt_flags[0])
    {
        _halt_flag = &_halt_flag[1];
    }
    else
    {
        _halt_flag = &_halt_flags[0];
    }
    *_halt_flag = true;
}

XenomaiWorkerThread::XenomaiWorkerThread(XenomaiBarrierWithTrigger& barrier,
                                               WorkerCallback callback,
                                               void*callback_data,
                                               std::atomic_bool& running_flag,
                                               int cpu_id) : _barrier(barrier),
                                                             _callback(callback),
                                                             _callback_data(callback_data),
                                                             _running(running_flag)
{
    struct sched_param rt_params = { .sched_priority = 75 };
    pthread_attr_t task_attributes;
    pthread_attr_init(&task_attributes);

    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(cpu_id, &cpus);
    pthread_attr_setaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);

    // TODO: better error checking and propagation
    int res = __cobalt_pthread_create(&_thread_handle, &task_attributes, &_worker_function, this);
    assert(res == 0);
}

XenomaiWorkerThread::~XenomaiWorkerThread()
{
    __cobalt_pthread_join(_thread_handle, nullptr);
}

void*XenomaiWorkerThread::_worker_function(void*data)
{
    reinterpret_cast<XenomaiWorkerThread*>(data)->_internal_worker_function();
    return nullptr;
}

void XenomaiWorkerThread::_internal_worker_function()
{
    // this is a realtime thread
    ThreadRtFlag rt_flag;
    while(true)
    {
        _barrier.wait();
        if (_running.load() == false)
        {
            // condition checked when coming out of wait as we might want to exit immediately here
            break;
        }
        _callback(_callback_data);
    }
}

XenomaiWorkerPool::~XenomaiWorkerPool()
{
    // wait for all workers to arrive at the barrier, then tell them to stop,
    // they should exit as soon as they are woken up by he scheduler.
    _barrier.wait_for_all();
    _running.store(false);
    _barrier.release_all();
}

int XenomaiWorkerPool::add_worker(WorkerCallback worker_cb, void* worker_data)
{
    _barrier.set_no_threads(_no_workers + 1);
    // round-robin assignment to cpu cores
    int core = _no_workers % _no_cores;
    _workers.push_back(std::make_unique<XenomaiWorkerThread>(_barrier, worker_cb, worker_data, _running, core));
    _no_workers++;
    // Wait until the thread is idle to avoid synchronisation issues
    _barrier.wait_for_all();
}

void XenomaiWorkerPool::wait_for_workers_idle()
{
    _barrier.wait_for_all();
}

void XenomaiWorkerPool::wakeup_workers()
{
    _barrier.release_all();
    _barrier.wait_for_all();
}


} // twine
#endif
#ifndef TWINE_BUILD_WITH_XENOMAI

#include "twine.h"
#include "xenomai_worker_pool.h"

/* Dummy implementation for when building without xenomai support */
namespace twine {
XenomaiBarrierWithTrigger::XenomaiBarrierWithTrigger() {}
XenomaiBarrierWithTrigger::~XenomaiBarrierWithTrigger() {}
void XenomaiBarrierWithTrigger::wait() {}
void XenomaiBarrierWithTrigger::wait_for_all() {}
void XenomaiBarrierWithTrigger::set_no_threads(int /*threads*/) {}
void XenomaiBarrierWithTrigger::release_all() {}

XenomaiWorkerThread::XenomaiWorkerThread(XenomaiBarrierWithTrigger& barrier, WorkerCallback /*callback*/,
                    void* /*callback_data*/, std::atomic_bool& running_flag, int /*id*/) : _barrier(barrier),
                                                                                           _running(running_flag){}
XenomaiWorkerThread::~XenomaiWorkerThread() {}
void* XenomaiWorkerThread::_worker_function(void* /*data*/) {}
void XenomaiWorkerThread::_internal_worker_function() {}

XenomaiWorkerPool::~XenomaiWorkerPool() {}

int XenomaiWorkerPool::add_worker(WorkerCallback /*worker_cb*/, void* /*worker_data**/) {assert(false);}
void XenomaiWorkerPool::wait_for_workers_idle(){}
void XenomaiWorkerPool::wakeup_workers() {}
}

#endif
