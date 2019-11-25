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
 * @brief Twine main source file
 * @copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifdef __SSE__
    #include <xmmintrin.h>
    #define denormals_intrinsic() _mm_setcsr(0x9FC0)
#else
    #define denormals_intrinsic()
#endif

#ifdef TWINE_BUILD_WITH_XENOMAI
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #include <cobalt/stdio.h>
    #pragma GCC diagnostic pop
#else
    #include <cstdio>
    #include <cstdarg>
    #define rt_vfprintf vfprintf
#endif

#include "twine/twine.h"
#include "twine_internal.h"
#include "worker_pool_implementation.h"

namespace twine {

constexpr int64_t NS_TO_S = 1'000'000'000;

thread_local int ThreadRtFlag::_instance_counter = 0;
bool XenomaiRtFlag::_enabled = false;
static XenomaiRtFlag running_xenomai_realtime;

bool is_current_thread_realtime()
{
    return ThreadRtFlag::is_realtime();
}

int rt_printf(const char *format, ...)
{
    va_list args;
    int n;

    va_start(args, format);
    n = rt_vfprintf(stdout, format, args);
    va_end(args);

    return n;
}

void init_xenomai()
{
    running_xenomai_realtime.set(true);
}

std::unique_ptr<WorkerPool> WorkerPool::create_worker_pool(int cores, bool disable_denormals)
{
    if (running_xenomai_realtime.is_set())
    {
        return std::make_unique<WorkerPoolImpl<ThreadType::XENOMAI>>(cores, disable_denormals);
    }
    return std::make_unique<WorkerPoolImpl<ThreadType::PTHREAD>>(cores, disable_denormals);
}

std::chrono::nanoseconds current_rt_time()
{
    if (running_xenomai_realtime.is_set())
    {
        timespec tp;
        __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);
        return std::chrono::nanoseconds(tp.tv_nsec + tp.tv_sec * NS_TO_S);
    }
    else
    {
        return std::chrono::high_resolution_clock::now().time_since_epoch();
    }
}

void set_flush_denormals_to_zero()
{
    denormals_intrinsic();
}


} // twine
