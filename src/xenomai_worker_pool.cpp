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

class XenomaiWorkerPool;

XenomaiWorkerBarrier::XenomaiWorkerBarrier()
{
    __cobalt_pthread_mutex_init(&mutex, nullptr);
    __cobalt_pthread_cond_init(&cond, nullptr);
    n_threads_on_barrier = 0;
}

XenomaiWorkerThread::XenomaiWorkerThread() : barriers(nullptr),
                                             aux_thread(0),
                                             callback(nullptr),
                                             callback_data(nullptr),
                                             thread_id(-1)
{}

void* _internal_worker_function(void* data)
{
    ThreadRtFlag rt_flag;

    XenomaiWorkerThread* worker_data = (XenomaiWorkerThread*) data;
    WorkerCallback user_callback = worker_data->callback;
    void* user_data = worker_data->callback_data;
    XenomaiBarriers* barriers = worker_data->barriers;

    // TODO: replace 1 with a condition that can be signaled by main thread
    while (1)
    {
        // TODO: even here, use better abstractions for barrier management

        // Set yourself as idle and signal to the main thread, when all threads are idle main will start
        __cobalt_pthread_mutex_lock(&barriers->currently_idle.mutex);
        barriers->currently_idle.n_threads_on_barrier++;
        __cobalt_pthread_cond_signal(&barriers->currently_idle.cond);
        __cobalt_pthread_mutex_unlock(&barriers->currently_idle.mutex);

        // wait for work from main
        __cobalt_pthread_mutex_lock(&barriers->work_ready.mutex);
        while (!barriers->work_ready.n_threads_on_barrier)
        {
            __cobalt_pthread_cond_wait(&barriers->work_ready.cond, &barriers->work_ready.mutex);
        }
        __cobalt_pthread_mutex_unlock(&barriers->work_ready.mutex);

        // Call the user-registered function to do the real work
        user_callback(user_data);

        // mark yourself as finished and signal to main
        __cobalt_pthread_mutex_lock(&barriers->currently_working.mutex);
        barriers->currently_working.n_threads_on_barrier--;
        __cobalt_pthread_cond_signal(&barriers->currently_working.cond);
        __cobalt_pthread_mutex_unlock(&barriers->currently_working.mutex);

        // Wait for permission to finish
        __cobalt_pthread_mutex_lock(&barriers->can_finish.mutex);
        while (!barriers->can_finish.n_threads_on_barrier)
        {
            __cobalt_pthread_cond_wait(&barriers->can_finish.cond, &barriers->can_finish.mutex);
        }
        __cobalt_pthread_mutex_unlock(&barriers->can_finish.mutex);

    }

    pthread_exit(NULL);
    return NULL;
}

static int _initialize_worker_thread(XenomaiWorkerThread* wthread)
{
    // TODO: pass prio as argument
    struct sched_param rt_params = { .sched_priority = 75 };
    pthread_attr_t task_attributes;
    __cobalt_pthread_attr_init(&task_attributes);

    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);
    // Distribute threads among cores in round-robin fashion
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(wthread->thread_id, &cpus);
    pthread_attr_setaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpus);

    // TODO: better error checking and propagation
    int res = __cobalt_pthread_create(&wthread->aux_thread, &task_attributes, &_internal_worker_function, wthread);

    return res;
}

XenomaiWorkerPool::XenomaiWorkerPool(int cores) : _n_cores(cores)
{}

XenomaiWorkerPool::~XenomaiWorkerPool()
{}

int XenomaiWorkerPool::add_worker(WorkerCallback worker_cb, void*worker_data)
{
    int thread_id = _n_workers;
    assert(thread_id < MAX_WORKERS_PER_POOL);

    XenomaiWorkerThread* wthread = &_worker_threads[thread_id];

    wthread->barriers = &_barriers;
    wthread->thread_id = thread_id;
    wthread->callback = worker_cb;
    wthread->callback_data = worker_data;
    // TODO: check return value
    _initialize_worker_thread(wthread);

    _n_workers++;

    // TODO: proper return value
    return 0;
}

void XenomaiWorkerPool::wait_for_workers_idle()
{
    // Make sure all workers are ready
    __cobalt_pthread_mutex_lock(&_barriers.currently_idle.mutex);
    while (_barriers.currently_idle.n_threads_on_barrier != _n_workers)
    {
        __cobalt_pthread_cond_wait(&_barriers.currently_idle.cond, &_barriers.currently_idle.mutex);
    }
    __cobalt_pthread_mutex_unlock(&_barriers.currently_idle.mutex);

    // All threads are now blocked; it's safe to not lock the mutex.
    // Prevent them from finishing before authorized.
    _barriers.can_finish.n_threads_on_barrier = 0;
    // Reset the number of currentlyWorking threads
    _barriers.currently_working.n_threads_on_barrier = _n_workers;
}

void XenomaiWorkerPool::wakeup_workers()
{
    // TODO: polish the code using a better barrier abstraction instead than repeated calls
    // The basic structure was adapted from:
    // https://stackoverflow.com/questions/12282393/how-to-synchronize-manager-worker-pthreads-without-a-join

    // TODO: the 1 here is not semantically a "n_threads_on_barrier"
    // Signal to the threads to start
    __cobalt_pthread_mutex_lock(&_barriers.work_ready.mutex);
    _barriers.work_ready.n_threads_on_barrier = 1;
    __cobalt_pthread_cond_broadcast(&_barriers.work_ready.cond);
    __cobalt_pthread_mutex_unlock(&_barriers.work_ready.mutex);

    // Wait for them to finish
    __cobalt_pthread_mutex_lock(&_barriers.currently_working.mutex);
    while (_barriers.currently_working.n_threads_on_barrier != 0)
    {
        __cobalt_pthread_cond_wait(&_barriers.currently_working.cond, &_barriers.currently_working.mutex);
    }
    __cobalt_pthread_mutex_unlock(&_barriers.currently_working.mutex);

    // The threads are now waiting for permission to finish
    // Prevent them from starting again
    _barriers.work_ready.n_threads_on_barrier = 0;
    _barriers.currently_idle.n_threads_on_barrier = 0;

    // TODO: the 1 here is not semantically a "n_threads_on_barrier"
    // Allow them to finish
    __cobalt_pthread_mutex_lock(&_barriers.can_finish.mutex);
    _barriers.can_finish.n_threads_on_barrier = 1;
    __cobalt_pthread_cond_broadcast(&_barriers.can_finish.cond);
    __cobalt_pthread_mutex_unlock(&_barriers.can_finish.mutex);
}

} // twine
#endif
#ifndef TWINE_BUILD_WITH_XENOMAI

#include "twine.h"
#include "xenomai_worker_pool.h"

/* Dummy implementation for when building without xenomai support */
namespace twine {
XenomaiWorkerBarrier::XenomaiWorkerBarrier() {}
XenomaiWorkerThread::XenomaiWorkerThread() {}
XenomaiWorkerPool::XenomaiWorkerPool(int /*cores*/) {assert(false);}
XenomaiWorkerPool::~XenomaiWorkerPool() {}

int XenomaiWorkerPool::add_worker(WorkerCallback /*worker_cb*/, void* /*worker_data**/) {}
void XenomaiWorkerPool::wait_for_workers_idle(){}
void XenomaiWorkerPool::wakeup_workers() {}
}

#endif
