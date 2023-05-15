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
 * @brief Twine implementation
 * @copyright 2017-2023 Elk Audio AB, Stockholm
 */

#ifndef TWINE_TWINE_INTERNAL_H
#define TWINE_TWINE_INTERNAL_H

namespace twine {
/**
 * @brief Signal to twine that Worker Pools should use the xenomai thread api and
 *        not the default std::thread implementation.
 *        Must be called before creating any Worker Pools. Not indented to be called
 *        from processors or plugins.
 */
void init_xenomai();

/**
 * @brief Used to signal that the current thread is a realtime thread
 */
class ThreadRtFlag
{
public:
    ThreadRtFlag()
    {
        _instance_counter += 1;
    }
    ~ThreadRtFlag()
    {
        _instance_counter -= 1;
    }

    static bool is_realtime()
    {
        return _instance_counter > 0;
    }

private:
    static thread_local int _instance_counter;
};

class XenomaiRtFlag
{
public:
    void set(bool enabled)
    {
        _enabled = enabled;
    }
    bool is_set()
    {
        return _enabled;
    }
private:
    static bool _enabled;
};

#define TWINE_DECLARE_NON_COPYABLE(type) type(const type& other) = delete; \
                                        type& operator=(const type&) = delete;

} // namespace twine
#endif //TWINE_TWINE_INTERNAL_H
