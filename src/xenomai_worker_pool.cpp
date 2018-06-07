
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cobalt/pthread.h>
#pragma GCC diagnostic pop


#include "worker_pool.h"

namespace yarn {

constexpr int MAX_WORKERS_PER_POOL = 8;
constexpr int N_CPU_CORES = 4;

/*********************
*  Data structures  *
*********************/

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int n_threads_on_barrier;

} WorkerBarrier;

typedef struct
{

    RaspaWorkerPool* pool;
    pthread_t aux_thread;
    RaspaWorkerFunction callback;
    void* callback_data;
    int thread_id;

} WorkerThread;

struct RaspaWorkerPool
{
    WorkerBarrier currently_idle;
    WorkerBarrier work_ready;
    WorkerBarrier currently_working;
    WorkerBarrier can_finish;

    WorkerThread worker_threads[MAX_WORKERS_PER_POOL];
    int n_workers;

};


/*******************************
*  Internal helper functions  *
*******************************/

static void* _internal_worker_function(void* data)
{
    WorkerThread* worker_data = (WorkerThread*) data;
    RaspaWorkerFunction user_callback = worker_data->callback;
    RaspaWorkerFunction user_data = worker_data->callback_data;
    RaspaWorkerPool* pool = worker_data->pool;

    // TODO: replace 1 with a condition that can be signaled by main thread
    while (1)
    {
        // TODO: even here, use better abstractions for barrier management

        // Set yourself as idle and signal to the main thread, when all threads are idle main will start
        __cobalt_pthread_mutex_lock(&pool->currently_idle.mutex);
        pool->currently_idle.n_threads_on_barrier++;
        __cobalt_pthread_cond_signal(&pool->currently_idle.cond);
        __cobalt_pthread_mutex_unlock(&pool->currently_idle.mutex);

        // wait for work from main
        __cobalt_pthread_mutex_lock(&pool->work_ready.mutex);
        while (!pool->work_ready.n_threads_on_barrier)
        {
            __cobalt_pthread_cond_wait(&pool->work_ready.cond, &pool->work_ready.mutex);
        }
        __cobalt_pthread_mutex_unlock(&pool->work_ready.mutex);

        // Call the user-registered function to do the real work
        user_callback(user_data);

        // mark yourself as finished and signal to main
        __cobalt_pthread_mutex_lock(&pool->currently_working.mutex);
        pool->currently_working.n_threads_on_barrier--;
        __cobalt_pthread_cond_signal(&pool->currently_working.cond);
        __cobalt_pthread_mutex_unlock(&pool->currently_working.mutex);

        // Wait for permission to finish
        __cobalt_pthread_mutex_lock(&pool->can_finish.mutex);
        while (!pool->can_finish.n_threads_on_barrier)
        {
            __cobalt_pthread_cond_wait(&pool->can_finish.cond, &pool->can_finish.mutex);
        }
        __cobalt_pthread_mutex_unlock(&pool->can_finish.mutex);

    }

    pthread_exit(NULL);
    return NULL;
}

static int _initialize_worker_barrier(WorkerBarrier* barrier)
{
    // TODO: check return codes and propagate them
    __cobalt_pthread_mutex_init(&barrier->mutex, NULL);
    __cobalt_pthread_cond_init(&barrier->cond, NULL);
    barrier->n_threads_on_barrier = 0;

    return 0;
}

static int _initialize_worker_thread(WorkerThread* wthread)
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

/************************
*  API implementation  *
************************/

RaspaWorkerPool* raspa_create_worker_pool()
{
    RaspaWorkerPool* pool = (RaspaWorkerPool*) malloc(sizeof(RaspaWorkerPool));

    pool->n_workers = 0;
    for (int i=0; i<MAX_WORKERS_PER_POOL; i++)
    {
        // TODO: check return arguments
        _initialize_worker_barrier(&pool->currently_idle);
        _initialize_worker_barrier(&pool->work_ready);
        _initialize_worker_barrier(&pool->currently_working);
        _initialize_worker_barrier(&pool->can_finish);

        WorkerThread* wthread = &pool->worker_threads[i];
        wthread->pool = pool;
        wthread->aux_thread = 0;
        wthread->thread_id = -1;
    }

    return pool;
}

int raspa_add_worker_to_pool(RaspaWorkerPool* pool,
                             RaspaWorkerFunction worker_function,
                             void* worker_data)
{
    // TODO : check on max workers per pool
    int tid = pool->n_workers;
    WorkerThread* wthread = &pool->worker_threads[tid];

    wthread->pool = pool;
    wthread->thread_id = tid;
    wthread->callback = worker_function;
    wthread->callback_data = worker_data;
    // TODO: check return value
    _initialize_worker_thread(wthread);

    pool->n_workers++;

    // TODO: proper return value
    return 0;
}

void raspa_wait_for_workers_idle(RaspaWorkerPool* pool)
{
    // Make sure all workers are ready
    __cobalt_pthread_mutex_lock(&pool->currently_idle.mutex);
    while (pool->currently_idle.n_threads_on_barrier != pool->n_workers)
    {
        __cobalt_pthread_cond_wait(&pool->currently_idle.cond, &pool->currently_idle.mutex);
    }
    __cobalt_pthread_mutex_unlock(&pool->currently_idle.mutex);

    // All threads are now blocked; it's safe to not lock the mutex.
    // Prevent them from finishing before authorized.
    pool->can_finish.n_threads_on_barrier = 0;
    // Reset the number of currentlyWorking threads
    pool->currently_working.n_threads_on_barrier = pool->n_workers;
}

void raspa_wakeup_workers(RaspaWorkerPool* pool)
{
    // TODO: polish the code using a better barrier abstraction instead than repeated calls
    // The basic structure was adapted from:
    // https://stackoverflow.com/questions/12282393/how-to-synchronize-manager-worker-pthreads-without-a-join

    // TODO: the 1 here is not semantically a "n_threads_on_barrier"
    // Signal to the threads to start
    __cobalt_pthread_mutex_lock(&pool->work_ready.mutex);
    pool->work_ready.n_threads_on_barrier = 1;
    __cobalt_pthread_cond_broadcast(&pool->work_ready.cond);
    __cobalt_pthread_mutex_unlock(&pool->work_ready.mutex);

    // Wait for them to finish
    __cobalt_pthread_mutex_lock(&pool->currently_working.mutex);
    while (pool->currently_working.n_threads_on_barrier != 0) {
        __cobalt_pthread_cond_wait(&pool->currently_working.cond, &pool->currently_working.mutex);
    }
    __cobalt_pthread_mutex_unlock(&pool->currently_working.mutex);

    // The threads are now waiting for permission to finish
    // Prevent them from starting again
    pool->work_ready.n_threads_on_barrier = 0;
    pool->currently_idle.n_threads_on_barrier = 0;

    // TODO: the 1 here is not semantically a "n_threads_on_barrier"
    // Allow them to finish
    __cobalt_pthread_mutex_lock(&pool->can_finish.mutex);
    pool->can_finish.n_threads_on_barrier = 1;
    __cobalt_pthread_cond_broadcast(&pool->can_finish.cond);
    __cobalt_pthread_mutex_unlock(&pool->can_finish.mutex);
}

void raspa_destroy_worker_pool(RaspaWorkerPool* pool)
{
    // TODO: signal workers to exit while () loop
    // TODO: join threads
    // TODO: destroy barriers and threads

    free(pool);
}

} // yarn