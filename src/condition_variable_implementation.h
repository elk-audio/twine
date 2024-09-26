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

#ifndef TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H
#define TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H

#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <condition_variable>
#include <exception>
#include <atomic>
#include <cstring>
#include <cassert>
#include <cstdlib>
#ifndef TWINE_WINDOWS_THREADING
#include <semaphore.h>
#include <fcntl.h>
#include <poll.h>
#endif
#include "twine_internal.h"

#ifdef TWINE_BUILD_WITH_XENOMAI
    #include <poll.h>
    #include <sys/eventfd.h>
    #include <rtdm/ipc.h>
    #include <cobalt/sys/socket.h>
#elif TWINE_BUILD_WITH_EVL
    #include <evl/xbuf.h>
    #include <evl/thread.h>
#endif

namespace twine {

/**
 * @brief Implementation with regular c++ std library constructs for
 *        use in a regular linux context.
 */
class StdConditionVariable : public RtConditionVariable
{
public:
    ~StdConditionVariable() override = default;

    void notify() override;

    bool wait() override;

private:
    bool                    _flag{false};
    std::mutex              _mutex;
    std::condition_variable _cond_var;
};

void StdConditionVariable::notify()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _flag = true;
    _cond_var.notify_one();
}

bool StdConditionVariable::wait()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _cond_var.wait(lock);
    bool notified = _flag;
    _flag = false;
    return notified;
}

/**
 * @brief Implementation using posix semaphores for use in regular linux and MacOs
 */

constexpr std::string_view COND_VAR_BASE_NAME = "/twine_cond_";
constexpr int MAX_RETRIES = 100;

#ifndef TWINE_WINDOWS_THREADING
class PosixSemaphoreConditionVariable : public RtConditionVariable
{
public:
    PosixSemaphoreConditionVariable();

    ~PosixSemaphoreConditionVariable() override;

    void notify() override;

    bool wait() override;

private:
    std::string   _name;
    sem_t*        _semaphore;
};

PosixSemaphoreConditionVariable::PosixSemaphoreConditionVariable() : _semaphore(nullptr)
{
    int retries = MAX_RETRIES;
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (--retries > 0)
    {
        // To avoid name collisions, each semaphore has a randomized suffix added to it
        std::string name = std::string(COND_VAR_BASE_NAME).append(std::to_string(std::rand()));
        _semaphore = sem_open(name.c_str(), O_CREAT | O_EXCL, 0, 0);
        if (_semaphore == SEM_FAILED)
        {
            if (errno != EEXIST)
            {
                auto err_str = std::string("Failed to initialize RtConditionVariable, ") + strerror(errno);
                throw std::runtime_error(err_str.c_str());
            }
            continue;
        }
        _name = name;
        return;
    }
    throw std::runtime_error("Failed to initialize RtConditionVariable, no more retries.");
}

PosixSemaphoreConditionVariable::~PosixSemaphoreConditionVariable()
{
    if (_semaphore)
    {
        this->notify();
        sem_unlink(_name.c_str());
    }
}

void PosixSemaphoreConditionVariable::notify()
{
    sem_post(_semaphore);
}

bool PosixSemaphoreConditionVariable::wait()
{
    sem_wait(_semaphore);
    return true;
}
#endif

/* The maximum number of condition variable instances depend on the
 * number of rtp file descriptors enabled in the the xenomai kernel.
 * It is set with CONFIG_XENO_OPT_PIPE_NRDEV or CONFIG_EVL_NR_XBUFS, pass the same value
 * to twine when building for xenomai */

constexpr size_t MAX_RT_COND_VARS = TWINE_MAX_RT_CONDITION_VARS;

// Note, static variables are guaranteed to be zero initialized
static std::array<bool, MAX_RT_COND_VARS> active_ids;
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
    assert(id < static_cast<int>(MAX_RT_COND_VARS));
    std::unique_lock<std::mutex> lock(mutex);
    active_ids[id] = false;
}

#ifdef TWINE_BUILD_WITH_XENOMAI
using MsgType = uint8_t;
using NonRTMsgType = uint64_t;

constexpr size_t NUM_ELEMENTS = 64;
constexpr int INFINITE_POLL_TIME = -1;

/**
 * @brief Implementation using xenomai xddp queues that allow signalling a
 *        non xenomai thread from a xenomai thread.
 */
class XenomaiConditionVariable : public RtConditionVariable
{
public:
    XenomaiConditionVariable(int id);

    virtual ~XenomaiConditionVariable() override;

    void notify() override;

    bool wait() override;

private:
    void _set_up_socket();
    void _set_up_files();

    std::string  _socket_name;
    sockaddr_ipc _socket_address;
    int          _socket_handle{0};

    int          _rt_file{0};
    int          _non_rt_file{0};
    int          _id{0};

    std::array<pollfd, 2> _poll_targets;
};

XenomaiConditionVariable::XenomaiConditionVariable(int id) : _id(id)
{
    _set_up_socket();
    _set_up_files();
}

XenomaiConditionVariable::~XenomaiConditionVariable()
{
    close(_rt_file);
    close(_non_rt_file);
    __cobalt_close(_socket_handle);
    deregister_id(_id);
}

void XenomaiConditionVariable::notify()
{
    if (ThreadRtFlag::is_realtime())
    {
        MsgType data = 1;
        __cobalt_sendto(_socket_handle, &data, sizeof(data), MSG_MORE, nullptr, 0);
    }
    else
    {
        // Linux EventFDs requires 8 bytes of data
        NonRTMsgType data = 1;
        [[maybe_unused]] auto unused = write(_non_rt_file, &data, sizeof(data));
    }
}

bool XenomaiConditionVariable::wait()
{
    MsgType buffer[NUM_ELEMENTS];
    poll(_poll_targets.data(), _poll_targets.size(), INFINITE_POLL_TIME);

    int len = 0;

    // drain file descriptors.
    for (auto& t : _poll_targets)
    {
        if (t.revents != 0)
        {
            len += read(t.fd, &buffer, sizeof(buffer));
            t.revents = 0;
        }
    }

    return len > 1;
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

void XenomaiConditionVariable::_set_up_files()
{
    _socket_name = "/dev/rtp" + std::to_string(_id);
    _non_rt_file = eventfd(0, EFD_SEMAPHORE);
    if (_non_rt_file <= 0)
    {
        throw std::runtime_error(strerror(errno));
    }

    _rt_file = open(_socket_name.c_str(), O_RDWR | O_NONBLOCK);
    if (_rt_file <= 0)
    {
        throw std::runtime_error(strerror(errno));
    }

    _poll_targets[0] = {.fd = _rt_file, .events = POLLIN, .revents = 0};
    _poll_targets[1] = {.fd = _non_rt_file, .events = POLLIN, .revents = 0};
}

#endif // TWINE_BUILD_WITH_XENOMAI

#ifdef TWINE_BUILD_WITH_EVL

using MsgType = uint8_t;
constexpr size_t XBUF_SIZE = 1024;
// since std::hardware_destructive_interference_size is not yet supported in GCC 11
constexpr int ASSUMED_CACHE_LINE_SIZE = 64;

constexpr auto EVL_COND_VAR_WAIT_TIMEOUT = std::chrono::milliseconds(1000);
constexpr int  EVL_SHUTDOWN_RETRIES = 10;

/**
 * @brief Implementation using EVL xbuf mechanisms
 */
class EvlConditionVariable : public RtConditionVariable
{
public:
    EvlConditionVariable(int id);

    virtual ~EvlConditionVariable() override;

    void notify() override;

    bool wait() override;

private:
    int  _xbuf_to_rt{0};
    int  _xbuf_to_nonrt{0};
    int  _id{0};
    alignas(ASSUMED_CACHE_LINE_SIZE) std::atomic_bool _is_waiting{false};
};

EvlConditionVariable::EvlConditionVariable(int id) : _id(id)
{
    _xbuf_to_rt = evl_create_xbuf(0, XBUF_SIZE, EVL_CLONE_PRIVATE, "twinecv-tort-buf-%d", _id);
     if (_xbuf_to_rt < 0)
     {
        throw std::runtime_error(strerror(errno));
     }

    _xbuf_to_nonrt = evl_create_xbuf(XBUF_SIZE, 0, EVL_CLONE_PRIVATE, "twinecv-tononrt-buf-%d", _id);
    if (_xbuf_to_rt < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
}

EvlConditionVariable::~EvlConditionVariable()
{
    close(_xbuf_to_rt);
    close(_xbuf_to_nonrt);
    int retries = EVL_SHUTDOWN_RETRIES;
    while (_is_waiting && retries > 0)
    {
        retries--;
        std::this_thread::sleep_for(EVL_COND_VAR_WAIT_TIMEOUT / (EVL_SHUTDOWN_RETRIES / 2));
    }

}

void EvlConditionVariable::notify()
{
    MsgType data = 1;
    if (!evl_is_inband())
    {
        oob_write(_xbuf_to_nonrt, &data, sizeof(data));
    }
    else
    {
        write(_xbuf_to_rt, &data, sizeof(data));
    }
}

bool EvlConditionVariable::wait()
{
    MsgType buffer;
    int len = 0;
    _is_waiting.store(true, std::memory_order_acquire);

    if (!evl_is_inband())
    {
        len += oob_read(_xbuf_to_rt, &buffer, sizeof(buffer));
    }
    else
    {
        while (true)
        {
            /* A read() call on an evl xbuf is blocking and won't unblock even if the xbuf fd is closed.
             * Hence we need to poll the fd order to be able to unblock threads waiting in wait() */
            pollfd fd = {.fd = _xbuf_to_nonrt, .events = POLLIN, .revents = 0} ;
            int res = poll(&fd, 1, EVL_COND_VAR_WAIT_TIMEOUT.count());

            if (res > 0 && fd.revents | POLLIN) // There was data to be read
            {
                len += read(_xbuf_to_nonrt, &buffer, sizeof(buffer));
                break;
            }
            else if (fd.revents | (POLLERR | POLLHUP | POLLNVAL)) // File descriptor closed or has errors, most likely because we're shutting down
            {
                len = 0;
                break;
            }
            // Else we timed out w/o errors, poll again.
        }
    }
    _is_waiting.store(false, std::memory_order_release);
    return len > 0;
}

#endif // TWINE_BUILD_WITH_EVL

}// namespace twine

#endif //TWINE_CONDITION_VARIABLE_IMPLEMENTATION_H

