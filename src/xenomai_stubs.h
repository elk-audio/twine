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
 * @brief Empty stubs for building without Xenomai support
 * @copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef TWINE_XENOMAI_STUBS_H
#define TWINE_XENOMAI_STUBS_H

#include <cassert>

namespace twine {

inline int __cobalt_pthread_mutex_init([[maybe_unused]]pthread_mutex_t* mutex, [[maybe_unused]]const pthread_mutexattr_t* attributes)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_mutex_destroy([[maybe_unused]] pthread_mutex_t* mutex)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_mutex_lock([[maybe_unused]] pthread_mutex_t* mutex)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_mutex_unlock([[maybe_unused]] pthread_mutex_t* mutex)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_cond_init([[maybe_unused]]pthread_cond_t* condition_var, [[maybe_unused]]const pthread_condattr_t* attributes)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_cond_destroy([[maybe_unused]]pthread_cond_t* condition_var)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_cond_wait([[maybe_unused]]pthread_cond_t* condition_var, [[maybe_unused]]pthread_mutex_t* mutex)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_cond_signal([[maybe_unused]]pthread_cond_t* condition_var)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_cond_broadcast([[maybe_unused]]pthread_cond_t* condition_var)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_create([[maybe_unused]]pthread_t* thread, [[maybe_unused]]const pthread_attr_t* attributes,
                            [[maybe_unused]]void *(*entry_fun) (void *), [[maybe_unused]]void* argument)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_join([[maybe_unused]]pthread_t thread, [[maybe_unused]]void** return_var = nullptr)
{
    assert(false);
    return 0;
}

inline int __cobalt_clock_gettime([[maybe_unused]] clockid_t clock_id, struct timespec *tp)
{
    assert(false);
    tp->tv_sec = 0;
    tp->tv_nsec = 0;
    return 0;
}

inline int __cobalt_sem_init([[maybe_unused]] sem_t* sem, [[maybe_unused]] int shared, [[maybe_unused]] int value)
{
    assert(false);
    return 0;
}

inline int __cobalt_sem_destroy([[maybe_unused]] sem_t* sem)
{
    assert(false);
    return 0;
}

inline int __cobalt_sem_wait([[maybe_unused]] sem_t* sem)
{
    assert(false);
    return 0;
}

inline int __cobalt_sem_post([[maybe_unused]] sem_t* sem)
{
    assert(false);
    return 0;
}

constexpr auto PTHREAD_WARNSW = 0;
inline void pthread_setmode_np([[maybe_unused]] int clrmask,[[maybe_unused]] int setmask, [[maybe_unused]] int* mode_r)
{
    assert(false);
}

}
#endif //TWINE_XENOMAI_STUBS_H
