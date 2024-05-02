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
