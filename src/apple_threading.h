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
 * @copyright 2017-2023 Elk Audio AB, Stockholm
 */

#ifndef TWINE_APPLE_THREADING_H
#define TWINE_APPLE_THREADING_H

#ifdef TWINE_APPLE_THREADING

#include <optional>
#include <string>

namespace twine {

#ifndef TWINE_BUILD_WITH_APPLE_COREAUDIO
typedef void* os_workgroup_join_token_s;
typedef void* os_workgroup_t;
#endif

namespace apple {

// CoreAudio is needed to fetch the thread workgroup for the audio device specified.
#ifdef TWINE_BUILD_WITH_APPLE_COREAUDIO

/**
 * @brief Given an audio output device name, this attempts to fetch and return an Apple audio thread workgroup.
 * @param device_name A validated audio device name.
 * @return A pair. First, the os_workgroup_t found. Apparently this can be nullptr, on failure.
 *                 Second, the status of the operation, useful for diagnosing/reporting failure.
 */
[[nodiscard]] std::pair<os_workgroup_t, AppleThreadingStatus> get_device_workgroup(const std::string& device_name);

/**
 * @brief This removes the current thread from a workgroup, if it has previously joined it.
 * Threads must leave all workgroups in the reverse order that they have joined them.
 * Failing to do so before exiting will result in undefined behavior.
 * @param join_token The join_token from when the current thread joined the workgroup.
 * @param p_workgroup The workgroup to leave.
 */
void leave_workgroup_if_needed(os_workgroup_join_token_s* join_token,
                               os_workgroup_t p_workgroup);
#endif

/**
 * @brief Sets the thread to realtime - with explicit periodicity defined for Apple.
 *        This is a prerequisite for it to then join the audio thread workgroup.
 * @param period_ms the thread period in ms.
 * @return status bool
 */
[[nodiscard]] bool set_current_thread_to_realtime(double period_ms);

/**
 * @brief Assuming the thread IS set to realtime, using the above 'set_current_thread_to_realtime',
 *        calling this method from the thread joins it to the audio thread workgroup if possible.
 *        If called with a non-realtime thread, joining will fail.
 * @param p_workgroup the workgroup to join.
 * @return The status of the initialization, and the join_token - which is only valid on success.
 *         The operation can fail at two stages:
 *         At setting the thread to real-time,
 *         and subsequently, at joining workgroup.
 */
[[nodiscard]] std::pair<AppleThreadingStatus, os_workgroup_join_token_s> join_workgroup(os_workgroup_t p_workgroup);

}} // Twine Apple namespace

#endif // TWINE_APPLE_THREADING

#endif // TWINE_APPLE_THREADING_H
