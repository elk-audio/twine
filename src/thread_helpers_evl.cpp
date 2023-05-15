#include "thread_helpers.h"

#include <unistd.h>
#include <cerrno>

#include <evl/clock.h>

namespace twine {


inline struct evl_mutex* to_evl_mutex(BaseMutex* mutex)
{
    return &(static_cast<EvlMutex*>(mutex)->mutex);
}

inline struct evl_event* to_evl_cond(BaseCondVar* cond_var)
{
    return &(static_cast<EvlCondVar*>(cond_var)->cond_var);
}

inline struct evl_sem* to_evl_sem(BaseSemaphore* semaphore)
{
    return &(static_cast<EvlSemaphore*>(semaphore)->semaphore);
}


int EvlThreadHelper::mutex_create(BaseMutex* mutex, [[maybe_unused]] const char* name)
{
    int fd = evl_new_mutex(to_evl_mutex(mutex), "%s-%d", name, gettid());
    if (fd < 0)
    {
        return fd;
    }
    fd = evl_open_mutex(to_evl_mutex(mutex), "%s-%d", name, gettid());
    if (fd < 0)
    {
        return fd;
    }

    return 0;
}

int EvlThreadHelper::mutex_destroy(BaseMutex* mutex)
{
    return evl_close_mutex(to_evl_mutex(mutex));
}

int EvlThreadHelper::mutex_lock(BaseMutex* mutex)
{
    return evl_lock_mutex(to_evl_mutex(mutex));
}

int EvlThreadHelper::mutex_unlock(BaseMutex* mutex)
{
    return evl_unlock_mutex(to_evl_mutex(mutex));
}

int EvlThreadHelper::condition_var_create(BaseCondVar* condition_var, [[maybe_unused]] const char* name)
{
    int fd = evl_new_event(to_evl_cond(condition_var), "%s-%d", name, gettid());
    if (fd < 0)
    {
        return fd;
    }
    fd = evl_open_event(to_evl_cond(condition_var), "%s-%d", name, gettid());
    if (fd < 0)
    {
        return fd;
    }

    return 0;
}

int EvlThreadHelper::condition_var_destroy(BaseCondVar* condition_var)
{
    return evl_close_event(to_evl_cond(condition_var));
}

int EvlThreadHelper::condition_wait(BaseCondVar* condition_var, BaseMutex* mutex)
{
    return evl_wait_event(to_evl_cond(condition_var), to_evl_mutex(mutex));
}

int EvlThreadHelper::condition_signal(BaseCondVar* condition_var)
{
    return evl_signal_event(to_evl_cond(condition_var));
}

int EvlThreadHelper::thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument)
{
    return pthread_create(thread, attributes, entry_fun, argument);
}

int EvlThreadHelper::thread_join(pthread_t thread, void** return_var)
{
    return pthread_join(thread, return_var);
}

int EvlThreadHelper::semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* name)
{
    auto e_sem = to_evl_sem(semaphore);
    int fd = evl_new_sem(e_sem, "%s-%d", name, gettid());
    if (fd < 0)
    {
        return fd;
    }
    fd = evl_open_sem(e_sem, "%s-%d", name, gettid());
    if (fd < 0)
    {
        return fd;
    }

    return 0;
}

int EvlThreadHelper::semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* name)
{
    return evl_close_sem(to_evl_sem(semaphore));
}

int EvlThreadHelper::semaphore_wait(BaseSemaphore* semaphore)
{
    return evl_get_sem(to_evl_sem(semaphore));
}

int EvlThreadHelper::semaphore_signal(BaseSemaphore* semaphore)
{
    return evl_put_sem(to_evl_sem(semaphore));
}


} // namespace twine

