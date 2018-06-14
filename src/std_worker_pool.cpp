#ifndef TWINE_XENOMAI_WORKER_POOL_H
#define TWINE_XENOMAI_WORKER_POOL_H

#include <iostream>
#include <cassert>

#include "std_worker_pool.h"
#include "twine_internal.h"

namespace twine {


BarrierWithTrigger::~BarrierWithTrigger()
{
    _thread_cond.notify_all();
    _calling_cond.notify_one();
}

void BarrierWithTrigger::wait()
{
    std::unique_lock lock(_thread_mutex);
    const bool& halt_flag = *_halt_flag; // 'local' halt flag for this round
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
    int current_threads = _no_threads_currently_on_barrier.load();
    if (current_threads == _no_threads)
    {
        return;
    }
    while (current_threads < _no_threads)
    {
        _calling_cond.wait(lock);
        current_threads = _no_threads_currently_on_barrier.load();
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
                                 std::atomic_bool& running_flag,
                                 int cpu_id ) : _barrier(barrier),
                                                _callback(callback),
                                                _callback_data(callback_data),
                                                _running(running_flag)
{
    // std::thread does not support setting affinity and priority so we
    // are forced to use a pthread here

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
    int res = pthread_create(&_thread_handle, &task_attributes, &_worker_function, this);
    assert(res == 0);
}


StdWorkerThread::~StdWorkerThread()
{
    if (_thread_handle != 0)
    {
        pthread_join(_thread_handle, nullptr);
    }
}

void* StdWorkerThread::_worker_function(void* data)
{
    reinterpret_cast<StdWorkerThread*>(data)->_internal_worker_function();
    return nullptr;
}

void StdWorkerThread::_internal_worker_function()
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

StdWorkerPool::~StdWorkerPool()
{
    // wait for all workers to arrive at the barrier, then tell them to stop,
    // they should exit as soon as they are woken up by he scheduler.
    _barrier.wait_for_all();
    _running.store(false);
    _barrier.relase_all();
}

int StdWorkerPool::add_worker(WorkerCallback worker_cb, void* worker_data)
{
    _barrier.set_no_threads(_no_workers + 1);
    // round-robin assignment to cpu cores
    int core = _no_workers % _no_cores;
    _workers.push_back(std::make_unique<StdWorkerThread>(_barrier, worker_cb, worker_data, _running, core));
    _no_workers++;
    // Wait until the thread is idle to avoid synchronisation issues
    _barrier.wait_for_all();
}

void StdWorkerPool::wait_for_workers_idle()
{
    _barrier.wait_for_all();
}

void StdWorkerPool::wakeup_workers()
{
    _barrier.relase_all();
    _barrier.wait_for_all();
}

}// namespace twine

#endif // TWINE_XENOMAI_WORKER_POOL_H