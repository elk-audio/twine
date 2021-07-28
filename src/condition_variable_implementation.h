/*
 * Copyright 2018-2020 Modern Ancient Instruments Networked AB, dba Elk
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

#ifndef TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H
#define TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H

#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <exception>
#include <cstring>
#include <cassert>

#ifdef TWINE_BUILD_WITH_XENOMAI
#include <cobalt/sys/socket.h>
#include <rtdm/ipc.h>
#include <sched.h>
#endif

#include "twine/twine.h"

namespace twine {

/**
 * @brief Implementation with regular c++ std library constructs for
 *        use in a regular linux context.
 */
class PosixConditionVariable : public RtConditionVariable
{
public:
    ~PosixConditionVariable() override = default;

    bool notify() override;

    bool wait() override;

private:
    bool                    _flag{false};
    std::mutex              _mutex;
    std::condition_variable _cond_var;
};

bool PosixConditionVariable::notify()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flag = true;
    _cond_var.notify_one();
    return _flag;
}

bool PosixConditionVariable::wait()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _cond_var.wait(lock);
    bool notified = _flag;
    _flag = false;
    return notified;
}

#ifdef TWINE_BUILD_WITH_XENOMAI
using MsgType = uint8_t;

constexpr size_t NUM_ELEMENTS = 64;

/**
 * @brief Implementation using xenomai xddp queues that allow signalling a
 *        non xenomai thread from a xenomai thread.
 */
class XenomaiConditionVariable : public RtConditionVariable
{
public:
    XenomaiConditionVariable(int id);

    virtual ~XenomaiConditionVariable() override;

    bool notify() override;

    bool wait() override;

private:
    void _set_up_socket();
    void _set_up_file();
    int  _get_sched_policy();

    std::string  _socket_name;
    sockaddr_ipc _socket_address;
    int          _socket_handle{0};
    int          _file{0};
    int          _id{0};
};

/* The maximum number of condition variable instances depend on the
 * number of rtp file descriptors enabled in the the xenomai kernel.
 * It is set with CONFIG_XENO_OPT_PIPE_NRDEV, pass the same value
 * to twine when building for xenomai */

constexpr size_t MAX_XENOMAI_DEVICES = TWINE_MAX_XENOMAI_RTP_DEVICES;

// Note, static variables are always zero initialized
static std::array<bool, MAX_XENOMAI_DEVICES> active_ids;
static std::mutex mutex;

int get_next_id()
{
    for (auto i = 0u; i < active_ids.size(); ++i)
    {
        if (active_ids[i] == false)
        {
            active_ids[i] = true;
            return i;
        }
    }
    throw std::runtime_error("Maximum number of RtConditionVariables reached");
}

void deregister_id(int id)
{
    assert(id < static_cast<int>(MAX_XENOMAI_DEVICES));
    std::unique_lock<std::mutex> lock(mutex);
    active_ids[id] = false;
}

XenomaiConditionVariable::XenomaiConditionVariable(int id) : _id(id)
{
    _set_up_socket();
    _set_up_file();
}

XenomaiConditionVariable::~XenomaiConditionVariable()
{
    deregister_id(_id);
}

int XenomaiConditionVariable::_get_sched_policy()
{
    int policy;
    pthread_t self = pthread_self();
    struct sched_param param;
    __cobalt_pthread_getschedparam(self, &policy, &param);
    return policy;
}

bool XenomaiConditionVariable::notify()
{
    MsgType data = 1;
    int ret = 0;
    int policy = _get_sched_policy();

    if (policy == SCHED_FIFO)
    {
        ret = __cobalt_sendto(_socket_handle, &data, sizeof(data), MSG_MORE, nullptr, 0);
    }
    else
    {
        ret = write(_file, &data, sizeof(data));
    }
    return ret > 0;
}

bool XenomaiConditionVariable::wait()
{
    MsgType buffer[NUM_ELEMENTS];
    int ret = 0;
    int policy = _get_sched_policy();

    if (policy == SCHED_FIFO)
    {
        // use recvfrom when in a xenomai real-time thread
        ret = __cobalt_recvfrom(_socket_handle, buffer, sizeof(buffer), 0, NULL, 0);
    }
    else
    {
        // If notify was called multiple times, we read them all in one go
        ret = read(_file, &buffer, sizeof(buffer));
    }

    return ret > 0;
}

void XenomaiConditionVariable::_set_up_socket()
{
    _socket_handle = __cobalt_socket(AF_RTIPC, SOCK_DGRAM, IPCPROTO_XDDP);
    if (_socket_handle < 0)
    {
        throw std::runtime_error("xddp support not enabled in kernel");
    }

    size_t pool_size = NUM_ELEMENTS * sizeof(MsgType);
    __cobalt_setsockopt(_socket_handle, SOL_XDDP, XDDP_BUFSZ, &pool_size, sizeof(pool_size));

    memset(&_socket_address, 0, sizeof(_socket_address));
    _socket_address.sipc_family = AF_RTIPC;
    _socket_address.sipc_port = _id;

    auto res = __cobalt_bind(_socket_handle, (struct sockaddr*) &_socket_address, sizeof(_socket_address));
    if (res < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
}

void XenomaiConditionVariable::_set_up_file()
{
    _socket_name = "/dev/rtp" + std::to_string(_id);
    _file = open(_socket_name.c_str(), O_RDWR);
    if (_file <= 0)
    {
        throw std::runtime_error(strerror(errno));
    }
}

#endif
}// namespace twine

#endif //TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H
