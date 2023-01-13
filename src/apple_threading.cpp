/*
 * Copyright 2018-2023 Elk Audio AB
 * Twine is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Twine is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with Twine.
 * If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief For Apple silicon, an additional API is exposed besides that for posix threading.
 *        This file collects tools for this Apple threading API.
 * @copyright 2017-2022 Elk Audio AB, Stockholm
 */

#include "twine/twine.h"
#include "apple_threading.h"

#ifdef TWINE_APPLE_THREADING

#include <pthread.h>


namespace twine::apple {

#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO

bool set_current_thread_to_realtime(double period_ms)
{
    const auto thread = pthread_self();

    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);

    const auto ticks_per_ms = (static_cast<double>(timebase.denom) * 1000000.0) /
                              static_cast<double>(timebase.numer);

    const auto period_ticks = static_cast<uint32_t>(std::min(static_cast<double>(std::numeric_limits<uint32_t>::max()),
                                                             period_ms * ticks_per_ms));

    thread_time_constraint_policy_data_t policy;

    // The nominal amount of time between separate processing arrivals.
    policy.period = period_ticks;

    // The maximum amount of real time that may elapse from the start of a separate processing arrival,
    // to the end of computation for logically correct functioning.
    policy.constraint = policy.period;

    // The nominal amount of computation time needed during a separate processing arrival.
    // The thread may be preempted after the computation time has elapsed.
    // If (computation < constraint/2) it will be forced to constraint/2
    // to avoid unintended preemption and associated timer interrupts.
    policy.computation = std::min(static_cast<uint32_t>(50000), policy.period);

    policy.preemptible = true;

    bool status = thread_policy_set(pthread_mach_thread_np(thread),
                                    THREAD_TIME_CONSTRAINT_POLICY,
                                    reinterpret_cast<thread_policy_t>(&policy),
                                    THREAD_TIME_CONSTRAINT_POLICY_COUNT);

    return status == KERN_SUCCESS;
}

std::pair<os_workgroup_t, AppleThreadingStatus> get_device_workgroup(const std::string& device_name)
{
    if (__builtin_available(macOS 11.00, *) == false)
    {
        return {nullptr, AppleThreadingStatus::MACOS_11_NOT_DETECTED};
    }

    AudioObjectPropertyAddress property_address;
    property_address.mSelector = kAudioHardwarePropertyDevices;
    property_address.mScope = kAudioObjectPropertyScopeWildcard;
    property_address.mElement = kAudioObjectPropertyElementMain;

    UInt32 size;
    OSStatus apple_oss_status;

    apple_oss_status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property_address, 0, nullptr, &size);
    if (apple_oss_status != noErr)
    {
        return {nullptr, AppleThreadingStatus::PD_SIZE_FAILED};
    }

    auto devices = std::make_unique<AudioDeviceID[]>(size);

    apple_oss_status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0, nullptr, &size, devices.get());
    if (apple_oss_status != noErr)
    {
        return {nullptr, AppleThreadingStatus::PD_FAILED};
    }

    // Calculating the number of audio devices.
    auto device_count = static_cast<int>(size) / static_cast<int>(sizeof(AudioDeviceID));

    for (int i = 0; i < device_count; ++i)
    {
        char name[512];
        size = sizeof(name);

        property_address.mSelector = kAudioDevicePropertyDeviceName;

        apple_oss_status = AudioObjectGetPropertyData(devices[i], &property_address, 0, nullptr, &size, name);
        if (apple_oss_status != noErr)
        {
            return {nullptr, AppleThreadingStatus::FETCH_NAME_FAILED};
        }

        auto name_string = std::string(name, static_cast<int>(strlen(name)));
        if (name_string == device_name)
        {
            property_address.mSelector = kAudioDevicePropertyIOThreadOSWorkgroup;

            apple_oss_status = AudioObjectGetPropertyDataSize(devices[i], &property_address, 0, nullptr, &size);
            if (apple_oss_status != noErr)
            {
                return {nullptr, AppleThreadingStatus::WG_SIZE_FAILED};
            }

            os_workgroup_t _Nonnull workgroup;

            apple_oss_status = AudioObjectGetPropertyData(devices[i], &property_address, 0, nullptr, &size, &workgroup);
            if (apple_oss_status != noErr)
            {
                return {nullptr, AppleThreadingStatus::WG_FAILED};
            }

            if (os_workgroup_testcancel(workgroup))
            {
                return {workgroup, AppleThreadingStatus::WG_CANCELLED};
            }

            // This is the only DESIRABLE outcome.
            return {workgroup, AppleThreadingStatus::OK};
        }
    }

    return {nullptr, AppleThreadingStatus::INVALID_DEVICE_NAME_PASSED};
}

void leave_workgroup_if_needed(os_workgroup_join_token_s* join_token,
                               os_workgroup_t p_workgroup)
{
    if (p_workgroup != nullptr)
    {
        os_workgroup_leave(p_workgroup, join_token);
    }
}

#endif

AppleThreadingStatus initialize_thread(os_workgroup_join_token_s* join_token,
                                       os_workgroup_t p_workgroup)
{
#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO
    if (p_workgroup == nullptr)
    {
        return AppleThreadingStatus::NO_WORKGROUP_PASSED;
    }
    else
    {
        bool workgroup_cancelled = os_workgroup_testcancel(p_workgroup);

        if (!workgroup_cancelled)
        {
            int result = os_workgroup_join(p_workgroup, join_token);

            switch (result)
            {
                case EINVAL:
                {
                    return AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED;
                }
                case EALREADY:
                {
                    // Attempting to join thread workgroup which thread is already member of.
                    // This isn't a problem which requires action. But good to assert for debugging.
                    assert(false);
                    return AppleThreadingStatus::OK;
                }
                default:
                {
                    return AppleThreadingStatus::OK;
                }
            }
        }
        else
        {
            return AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED;
        }
    }
#else
    return AppleThreadingStatus::OK;
#endif
}

std::string statusToString(AppleThreadingStatus status)
{
    switch (status)
    {
        case AppleThreadingStatus::OK:
            return "Successfully fetched the audio workgroup";
        case AppleThreadingStatus::WG_CANCELLED:
            return "The fetched audio workgroup has been cancelled";
        case AppleThreadingStatus::WG_FAILED:
            return "Failed fetching the audio workgroup";
        case AppleThreadingStatus::WG_SIZE_FAILED:
            return "Failed fetching the audio workgroup property data size";
        case AppleThreadingStatus::FETCH_NAME_FAILED:
            return "Failed fetching an audio device name";
        case AppleThreadingStatus::PD_FAILED:
            return "Failed fetching the kAudioObjectSystemObject property data";
        case AppleThreadingStatus::PD_SIZE_FAILED:
            return "Failed fetching the kAudioObjectSystemObject property data size";
        case AppleThreadingStatus::MACOS_11_NOT_DETECTED:
            return "MacOS version 11.0 and up is required to fetch workgroup info for a device";
        case AppleThreadingStatus::REALTIME_OK:
            return "Setting Apple thread realtime status succeeded";
        case AppleThreadingStatus::REALTIME_FAILED:
            return "Failed setting thread realtime status";
        case AppleThreadingStatus::NO_WORKGROUP_PASSED:
            return "No Apple real-time workgroup was passed";
        case AppleThreadingStatus::WORKGROUP_ALREADY_CANCELLED:
            return "Attempting to join thread workgroup that is already canceled";
        case AppleThreadingStatus::QOS_EAGAIN:
            return "EAGAIN: Resource temporarily unavailable";
        case AppleThreadingStatus::QOS_EPERM:
            return "EPERM: Operation not permitted";
        case AppleThreadingStatus::QOS_EINVAL:
            return "EINVAL: Invalid argument";
        case AppleThreadingStatus::QOS_UNKNOWN:
            return "Unknown error setting Apple thread QoS Class";
        default:
            return "";
    }
}

} // twine::apple namespace

#endif // TWINE_APPLE_THREADING

