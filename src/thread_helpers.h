/*
 * Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
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
 * @copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef TWINE_THREAD_HELPERS_H
#define TWINE_THREAD_HELPERS_H

#include <cassert>

#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#ifdef TWINE_BUILD_WITH_XENOMAI
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <cobalt/pthread.h>
#include <cobalt/semaphore.h>
#pragma GCC diagnostic pop
#endif
#ifndef TWINE_BUILD_WITH_XENOMAI
#include "xenomai_stubs.h"
#endif

#include "twine/twine.h"

namespace twine {

enum class ThreadType : uint32_t
{
    PTHREAD,
    XENOMAI
};

template<ThreadType type>
inline int mutex_create(pthread_mutex_t* mutex, const pthread_mutexattr_t* attributes)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_mutex_init(mutex, attributes);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_mutex_init(mutex, attributes);
    }
}

template<ThreadType type>
inline int mutex_destroy(pthread_mutex_t* mutex)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_mutex_destroy(mutex);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_mutex_destroy(mutex);
    }
}

template<ThreadType type>
inline int mutex_lock(pthread_mutex_t* mutex)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_mutex_lock(mutex);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_mutex_lock(mutex);
    }
}

template<ThreadType type>
inline int mutex_unlock(pthread_mutex_t* mutex)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_mutex_unlock(mutex);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_mutex_unlock(mutex);
    }
}

template<ThreadType type>
inline int condition_var_create(pthread_cond_t* condition_var, const pthread_condattr_t* attributes)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_cond_init(condition_var, attributes);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_cond_init(condition_var, attributes);
    }
}

template<ThreadType type>
inline int condition_var_destroy(pthread_cond_t* condition_var)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_cond_destroy(condition_var);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_cond_destroy(condition_var);
    }
}

template<ThreadType type>
inline int condition_wait(pthread_cond_t*condition_var, pthread_mutex_t* mutex)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_cond_wait(condition_var, mutex);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_cond_wait(condition_var, mutex);
    }
}

template<ThreadType type>
inline int condition_signal(pthread_cond_t* condition_var)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_cond_signal(condition_var);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_cond_signal(condition_var);
    }
}

template<ThreadType type>
inline int condition_broadcast(pthread_cond_t* condition_var)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_cond_broadcast(condition_var);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_cond_broadcast(condition_var);
    }
}

template<ThreadType type>
inline int thread_create(pthread_t* thread, const pthread_attr_t* attributes, void *(*entry_fun) (void *), void* argument)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_create(thread, attributes, entry_fun, argument);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_create(thread, attributes, entry_fun, argument);
    }
}

template<ThreadType type>
inline int thread_join(pthread_t thread, void** return_var = nullptr)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return pthread_join(thread, return_var);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_pthread_join(thread, return_var);
    }
}

template<ThreadType type>
inline int semaphore_create(sem_t** semaphore, [[maybe_unused]] const char* semaphore_name)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        sem_unlink(semaphore_name);
        *semaphore = sem_open(semaphore_name, O_CREAT, 0, 0);
        return 0;
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_sem_init(*semaphore, 0, 0);
    }
}

template<ThreadType type>
inline int semaphore_destroy(sem_t* semaphore, [[maybe_unused]] const char* semaphore_name)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        sem_unlink(semaphore_name);
        sem_close(semaphore);
        return 0;
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_sem_destroy(semaphore);
    }
}

template<ThreadType type>
inline int semaphore_wait(sem_t* semaphore)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return sem_wait(semaphore);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_sem_wait(semaphore);
    }
}

template<ThreadType type>
inline int semaphore_signal(sem_t* semaphore)
{
    if constexpr (type == ThreadType::PTHREAD)
    {
        return sem_post(semaphore);
    }
    else if constexpr (type == ThreadType::XENOMAI)
    {
        return __cobalt_sem_post(semaphore);
    }
}

} // namespace twine

#endif //TWINE_THREAD_HELPERS_H
