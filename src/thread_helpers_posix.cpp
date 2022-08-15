#include "thread_helpers.h"

#include <cerrno>

namespace twine {

BaseThreadHelper::~BaseThreadHelper()
{}

BaseMutex::~BaseMutex()
{}

BaseCondVar::~BaseCondVar()
{}

BaseSemaphore::~BaseSemaphore()
{}


int PosixThreadHelper::mutex_create(BaseMutex* mutex)
{
    return pthread_mutex_init(to_posix_mutex(mutex), nullptr);
}

int PosixThreadHelper::mutex_destroy(BaseMutex* mutex)
{
    return pthread_mutex_destroy(to_posix_mutex(mutex));
}

int PosixThreadHelper::mutex_lock(BaseMutex* mutex)
{
    return pthread_mutex_lock(to_posix_mutex(mutex));
}

int PosixThreadHelper::mutex_unlock(BaseMutex* mutex)
{
    return pthread_mutex_unlock(to_posix_mutex(mutex));
}

int PosixThreadHelper::condition_var_create(BaseCondVar* condition_var)
{
    return pthread_cond_init(to_posix_cond(condition_var), nullptr);
}

int PosixThreadHelper::condition_var_destroy(BaseCondVar* condition_var)
{
    return pthread_cond_destroy(to_posix_cond(condition_var));
}

int PosixThreadHelper::condition_wait(BaseCondVar* condition_var, BaseMutex* mutex)
{
    return pthread_cond_wait(to_posix_cond(condition_var), to_posix_mutex(mutex));
}

int PosixThreadHelper::condition_signal(BaseCondVar* condition_var)
{
    return pthread_cond_signal(to_posix_cond(condition_var));
}

int PosixThreadHelper::thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument)
{
    return pthread_create(thread, attributes, entry_fun, argument);
}

int PosixThreadHelper::thread_join(pthread_t thread, void** return_var)
{
    return pthread_join(thread, return_var);
}

int PosixThreadHelper::semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* semaphore_name)
{
    sem_unlink(semaphore_name);
    auto psemaphore = to_posix_sem(semaphore);
    psemaphore = sem_open(semaphore_name, O_CREAT, 0, 0);
    if (psemaphore == SEM_FAILED)
    {
        return errno;
    }
    return 0;
}

int PosixThreadHelper::semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* semaphore_name)
{
    sem_unlink(semaphore_name);
    sem_close(to_posix_sem(semaphore));
    return 0;
}

int PosixThreadHelper::semaphore_wait(BaseSemaphore* semaphore)
{
    return sem_wait(to_posix_sem(semaphore));
}

int PosixThreadHelper::semaphore_signal(BaseSemaphore* semaphore)
{
    return sem_post(to_posix_sem(semaphore));
}


} // namespace twine

