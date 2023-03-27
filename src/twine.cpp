/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
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
 * @copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
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
#elif TWINE_BUILD_WITH_EVL
    #include <evl/proxy.h>
    #include <evl/clock.h>
#else
    #include <cstdio>
    #include <cstdarg>
    #define rt_vfprintf vfprintf // TODO - really needed?
#endif

#include "twine/twine.h"
#include "twine_internal.h"
#include "twine_version.h"
#include "worker_pool_implementation.h"

#ifndef TWINE_ENABLE_CONDITION_VARIABLE
    #include "condition_variable_implementation.h"
#endif

namespace twine {

constexpr int64_t NS_TO_S = 1'000'000'000;

thread_local int ThreadRtFlag::_instance_counter = 0;
bool XenomaiRtFlag::_enabled = false;
static XenomaiRtFlag running_xenomai_realtime;

#define _STRINGIZE(X) #X
#define STRINGIZE(X) _STRINGIZE(X)

VersionInfo twine_version()
{
    return {TWINE__VERSION_MAJ, TWINE__VERSION_MIN, TWINE__VERSION_REV};
}

const char* build_info()
{
    return "Twine - version " STRINGIZE(TWINE__VERSION_MAJ) "." STRINGIZE(TWINE__VERSION_MIN) "." STRINGIZE(TWINE__VERSION_REV)
    ", built on " STRINGIZE(TWINE_BUILD_TIMESTAMP) " from commit: " STRINGIZE(TWINE_GIT_COMMIT_HASH);

}

bool is_current_thread_realtime()
{
    return ThreadRtFlag::is_realtime();
}

#ifdef TWINE_BUILD_WITH_XENOMAI
int rt_printf(const char *format, ...)
{
    va_list args;
    int n;

    va_start(args, format);
    n = rt_vfprintf(stdout, format, args);
    va_end(args);

    return n;
}
#elif TWINE_BUILD_WITH_EVL
    #define rt_printf evl_printf
#else
    #define rt_printf printf
#endif

void init_xenomai()
{
#if defined(TWINE_BUILD_WITH_XENOMAI) || defined(TWINE_BUILD_WITH_EVL)
    running_xenomai_realtime.set(true);
#endif
}

std::unique_ptr<WorkerPool> WorkerPool::create_worker_pool(int cores,
                                                           [[maybe_unused]] apple::AppleMultiThreadData apple_data,
                                                           bool disable_denormals,
                                                           bool break_on_mode_sw)
{
#ifdef TWINE_BUILD_WITH_XENOMAI
    if (running_xenomai_realtime.is_set())
	// TODO - add apple_data arguments - or maybe not?
    {
        return std::make_unique<WorkerPoolImpl<ThreadType::COBALT>>(cores, apple_data, disable_denormals, break_on_mode_sw);
    }
#elif TWINE_BUILD_WITH_EVL
    if (running_xenomai_realtime.is_set())
    {
        return std::make_unique<WorkerPoolImpl<ThreadType::EVL>>(cores, apple_data, disable_denormals, break_on_mode_sw);
    }
#endif
    return std::make_unique<WorkerPoolImpl<ThreadType::PTHREAD>>(cores, apple_data, disable_denormals, break_on_mode_sw);
}

std::chrono::nanoseconds current_rt_time()
{
    if (running_xenomai_realtime.is_set())
    {
#ifdef TWINE_BUILD_WITH_XENOMAI
        timespec tp;
        __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);
        return std::chrono::nanoseconds(tp.tv_nsec + tp.tv_sec * NS_TO_S);
#elif TWINE_BUILD_WITH_EVL
        timespec tp;
        evl_read_clock(EVL_CLOCK_MONOTONIC, &tp);
        return std::chrono::nanoseconds(tp.tv_nsec + tp.tv_sec * NS_TO_S);
#else
        assert(false && "Xenomai realtime set without a RT build");
        return std::chrono::nanoseconds(0);
#endif
    }
    else
    {
        return std::chrono::steady_clock::now().time_since_epoch();
    }
}

void set_flush_denormals_to_zero()
{
    denormals_intrinsic();
}

std::string to_error_string(twine::WorkerPoolStatus status)
{
    switch (status)
    {
        case twine::WorkerPoolStatus::PERMISSION_DENIED:
            return "Permission denied";

        case twine::WorkerPoolStatus::LIMIT_EXCEEDED:
            return "Thread count limit exceeded";

        default:
            return "Error";
    }
}

#ifndef TWINE_ENABLE_CONDITION_VARIABLE
std::unique_ptr<RtConditionVariable> RtConditionVariable::create_rt_condition_variable()
{
#ifdef TWINE_BUILD_WITH_XENOMAI
    if (running_xenomai_realtime.is_set())
    {
        int id = get_next_id();
        return std::make_unique<XenomaiConditionVariable>(id);
    }
#endif

    return std::make_unique<PosixSemaphoreConditionVariable>();
}
#endif

} // twine
