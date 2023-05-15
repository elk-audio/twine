/*
 * Copyright Copyright 2017-2023 Elk Audio AB
 * Twine is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Twine is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with Twine.
 * If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief Templated helper functions to make the Workerpool implementation thread-agnostic.
 * @copyright 2017-2023 Elk Audio AB, Stockholm
 */

#ifndef TWINE_THREAD_HELPERS_H
#define TWINE_THREAD_HELPERS_H

#include <cassert>

#include <cstdint>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#ifdef TWINE_BUILD_WITH_EVL
    #include <evl/evl.h>
    #include <evl/clock.h>
    #include <evl/mutex.h>
#endif


namespace twine {

enum class ThreadType : uint32_t
{
    PTHREAD,
    COBALT,
    EVL
};


// Empty base classes for the generic arguments of ThreadHelper

struct BaseMutex
{
    virtual ~BaseMutex() = 0;
};

struct BaseCondVar
{
    virtual ~BaseCondVar() = 0;
};

struct BaseSemaphore
{
    virtual ~BaseSemaphore() = 0;
};


/**
 * @brief Stateless pure virtual interface for wrapping thread functions
 *        implemented by the different libraries (POSIX, cobalt, EVL)
 */
class BaseThreadHelper
{
public:
    virtual ~BaseThreadHelper();

    virtual int mutex_create(BaseMutex* mutex, [[maybe_unused]] const char* name) = 0;

    virtual int mutex_destroy(BaseMutex* mutex) = 0;

    virtual int mutex_lock(BaseMutex* mutex) = 0;

    virtual int mutex_unlock(BaseMutex* mutex) = 0;

    virtual int condition_var_create(BaseCondVar* condition_var, [[maybe_unused]] const char* name) = 0;

    virtual int condition_var_destroy(BaseCondVar* condition_var) = 0;

    virtual int condition_wait(BaseCondVar* condition_var, BaseMutex* mutex) = 0;

    virtual int condition_signal(BaseCondVar* condition_var) = 0;

    virtual int thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument) = 0;

    virtual int thread_join(pthread_t thread, void** return_var) = 0;

    virtual int semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) = 0;

    virtual int semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) = 0;

    virtual int semaphore_wait(BaseSemaphore* semaphore) = 0;

    virtual int semaphore_signal(BaseSemaphore* semaphore) = 0;
};


class PosixThreadHelper : public BaseThreadHelper
{
public:
    PosixThreadHelper() = default;
    virtual ~PosixThreadHelper() override {};


    int mutex_create(BaseMutex* mutex, [[maybe_unused]] const char* name) override;

    int mutex_destroy(BaseMutex* mutex) override;

    int mutex_lock(BaseMutex* mutex) override;

    int mutex_unlock(BaseMutex* mutex) override;

    int condition_var_create(BaseCondVar* condition_var, [[maybe_unused]] const char* name) override;

    int condition_var_destroy(BaseCondVar* condition_var) override;

    int condition_wait(BaseCondVar* condition_var, BaseMutex* mutex) override;

    int condition_signal(BaseCondVar* condition_var) override;

    int thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument) override;

    int thread_join(pthread_t thread, void** return_var) override;

    int semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) override;

    int semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) override;

    int semaphore_wait(BaseSemaphore* semaphore) override;

    int semaphore_signal(BaseSemaphore* semaphore) override;
};

// These are shared between POSIX and Cobalt,
// and cause no issues for any build configuration, so it's fine to declare them here

struct PosixMutex : BaseMutex
{
    pthread_mutex_t mutex;
};

struct PosixCondVar : BaseCondVar
{
    pthread_cond_t cond_var;
};

struct PosixSemaphore : BaseSemaphore
{
    sem_t semaphore;
};

inline pthread_mutex_t* to_posix_mutex(BaseMutex* mutex)
{
    return &static_cast<PosixMutex*>(mutex)->mutex;
}

inline pthread_cond_t* to_posix_cond(BaseCondVar* cond_var)
{
    return &static_cast<PosixCondVar*>(cond_var)->cond_var;
}

inline sem_t* to_posix_sem(BaseSemaphore* semaphore)
{
    return &static_cast<PosixSemaphore*>(semaphore)->semaphore;
}


#ifdef TWINE_BUILD_WITH_XENOMAI

class CobaltThreadHelper : public BaseThreadHelper
{
public:
    CobaltThreadHelper() = default;
    virtual ~CobaltThreadHelper() override {};

    int mutex_create(BaseMutex* mutex, [[maybe_unused]] const char* name) override;

    int mutex_destroy(BaseMutex* mutex) override;

    int mutex_lock(BaseMutex* mutex) override;

    int mutex_unlock(BaseMutex* mutex) override;

    int condition_var_create(BaseCondVar* condition_var, [[maybe_unused]] const char* name) override;

    int condition_var_destroy(BaseCondVar* condition_var) override;

    int condition_wait(BaseCondVar* condition_var, BaseMutex* mutex) override;

    int condition_signal(BaseCondVar* condition_var) override;

    int thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument) override;

    int thread_join(pthread_t thread, void** return_var) override;

    int semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) override;

    int semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) override;

    int semaphore_wait(BaseSemaphore* semaphore) override;

    int semaphore_signal(BaseSemaphore* semaphore) override;
};

#endif // TWINE_BUILD_WITH_XENOMAI

#ifdef TWINE_BUILD_WITH_EVL

struct EvlMutex : BaseMutex
{
    struct evl_mutex mutex;
};

struct EvlCondVar : BaseCondVar
{
    struct evl_event cond_var;
};

struct EvlSemaphore : BaseSemaphore
{
    struct evl_sem semaphore;
};

class EvlThreadHelper : public BaseThreadHelper
{
public:
    int mutex_create(BaseMutex* mutex, [[maybe_unused]] const char* name) override;

    int mutex_destroy(BaseMutex* mutex) override;

    int mutex_lock(BaseMutex* mutex) override;

    int mutex_unlock(BaseMutex* mutex) override;

    int condition_var_create(BaseCondVar* condition_var, [[maybe_unused]] const char* name) override;

    int condition_var_destroy(BaseCondVar* condition_var) override;

    int condition_wait(BaseCondVar* condition_var, BaseMutex* mutex) override;

    int condition_signal(BaseCondVar* condition_var) override;

    int thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument) override;

    int thread_join(pthread_t thread, void** return_var = nullptr) override;

    int semaphore_create(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) override;

    int semaphore_destroy(BaseSemaphore* semaphore, [[maybe_unused]] const char* name) override;

    int semaphore_wait(BaseSemaphore* semaphore) override;

    int semaphore_signal(BaseSemaphore* semaphore) override;
};

#endif // TWINE_BUILD_WITH_EVL

} // namespace twine

#endif //TWINE_THREAD_HELPERS_H
