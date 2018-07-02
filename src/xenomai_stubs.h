#ifndef TWINE_XENOMAI_STUBS_H
#define TWINE_XENOMAI_STUBS_H

#include <cassert>

namespace twine {

inline int __cobalt_pthread_mutex_init([[maybe_unused]]pthread_mutex_t*mutex, [[maybe_unused]]const pthread_mutexattr_t* attributes)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_mutex_destroy([[maybe_unused]] pthread_mutex_t*mutex)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_mutex_lock([[maybe_unused]] pthread_mutex_t*mutex)
{
    assert(false);
    return 0;
}
inline int __cobalt_pthread_mutex_unlock([[maybe_unused]] pthread_mutex_t*mutex)
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

}

#endif //TWINE_XENOMAI_STUBS_H
