#ifndef TWINE_XENOMAI_WORKER_POOL_H
#define TWINE_XENOMAI_WORKER_POOL_H

#include <iostream>
#include <cassert>

#include "std_worker_pool.h"

namespace twine {


BarrierWithTrigger::~BarrierWithTrigger()
{
    _thread_cond.notify_all();
    _calling_cond.notify_one();
}

void BarrierWithTrigger::wait()
{
    std::unique_lock lock(_thread_mutex);
    const bool& halt_flag = *_halt_flag;
    _no_threads_currently_on_barrier++;

    if (_no_threads_currently_on_barrier == _no_threads)
    {
        /* Last thread to finish notifies any waiting thread */
        std::unique_lock<std::mutex> notifying_lock(_calling_mutex);
        _calling_cond.notify_one();
    }
    while (halt_flag)
    {
        _thread_cond.wait(lock);
    }
}

void BarrierWithTrigger::wait_for_all()
{
    std::unique_lock<std::mutex> lock(_calling_mutex);
    if (_no_threads_currently_on_barrier == _no_threads)
    {
        return;
    }
    while (_no_threads_currently_on_barrier < _no_threads)
    {
        _calling_cond.wait(lock);
    }
}

void BarrierWithTrigger::set_no_threads(int threads)
{
    _no_threads.store(threads);
}

void BarrierWithTrigger::relase_all()
{
    assert(_no_threads_currently_on_barrier == _no_threads);
    std::unique_lock lock(_thread_mutex);
    _swap_halt_flags();
    _no_threads_currently_on_barrier = 0;
    _thread_cond.notify_all();
}

/*
 *  The reason we need 2 halt flags is because if threads are released sequentially
 *  there is a possibility the first thread might finish before the last one is even
 *  notified. Since the half flag is common for all threads, there needs to be a
 *  separate halt flag 'per round'. But 2 is enough as we can reuse them.
 *
 *  Note, halt flags must only be switched when all threads are waiting to be released
 *  otherwise all synchronisation is off.
 */
void BarrierWithTrigger::_swap_halt_flags()
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

StdWorkerThread::StdWorkerThread(BarrierWithTrigger& barrier,
                                 WorkerCallback callback,
                                 void* callback_data,
                                 std::atomic_bool& running_flag) : _barrier(barrier),
                                                                   _callback(callback),
                                                                   _callback_data(callback_data),
                                                                   _running(running_flag)
{
    // TODO-start thread, set affinity etc
}


StdWorkerThread::~StdWorkerThread()
{}

void StdWorkerThread::worker()
{

}

int StdWorkerPool::add_worker(WorkerCallback worker_cb, void* worker_data)
{
    return 0;
}

void StdWorkerPool::wait_for_workers_idle()
{

}

void StdWorkerPool::raspa_wakeup_workers()
{

}
}// namespace twine

#endif // TWINE_XENOMAI_WORKER_POOL_H