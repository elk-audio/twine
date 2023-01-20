#include "thread_helpers.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cobalt/pthread.h>
#include <cobalt/semaphore.h>

namespace twine {

int CobaltThreadHelper::mutex_create(BaseMutex* mutex)
{
    return __cobalt_pthread_mutex_init(to_posix_mutex(mutex), nullptr);
}

int CobaltThreadHelper::mutex_destroy(BaseMutex* mutex)
{
    return __cobalt_pthread_mutex_destroy(to_posix_mutex(mutex));
}

int CobaltThreadHelper::mutex_lock(BaseMutex* mutex)
{
    return __cobalt_pthread_mutex_lock(to_posix_mutex(mutex));
}

int CobaltThreadHelper::mutex_unlock(BaseMutex* mutex)
{
    return __cobalt_pthread_mutex_unlock(to_posix_mutex(mutex));
}

int CobaltThreadHelper::condition_var_create(BaseCondVar* condition_var)
{
    return __cobalt_pthread_cond_init(to_posix_cond(condition_var), nullptr);
}

int CobaltThreadHelper::condition_var_destroy(BaseCondVar* condition_var)
{
    return __cobalt_pthread_cond_destroy(to_posix_cond(condition_var));
}

int CobaltThreadHelper::condition_wait(BaseCondVar* condition_var, BaseMutex* mutex)
{
    return __cobalt_pthread_cond_wait(to_posix_cond(condition_var), to_posix_mutex(mutex));
}

int CobaltThreadHelper::condition_signal(BaseCondVar* condition_var)
{
    return __cobalt_pthread_cond_signal(to_posix_cond(condition_var));
}

int CobaltThreadHelper::thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument)
{
    return __cobalt_pthread_create(thread, attributes, entry_fun, argument);
}

int CobaltThreadHelper::thread_join(pthread_t thread, void** return_var)
{
    return __cobalt_pthread_join(thread, return_var);
}

int CobaltThreadHelper::semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* semaphore_name)
{
    return __cobalt_sem_init(to_posix_sem(semaphore), 0, 0);
}

int CobaltThreadHelper::semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* semaphore_name)
{
    return __cobalt_sem_destroy(to_posix_sem(semaphore));
}

int CobaltThreadHelper::semaphore_wait(BaseSemaphore* semaphore)
{
    return __cobalt_sem_wait(to_posix_sem(semaphore));
}

int CobaltThreadHelper::semaphore_signal(BaseSemaphore* semaphore)
{
    return __cobalt_sem_post(to_posix_sem(semaphore));
}


} // namespace twine

