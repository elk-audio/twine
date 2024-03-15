/*
 * Copyright 2018-2024 Elk Audio AB
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
 * @brief This file collects defines and classes for windows
 * @copyright 2017-2024 Elk Audio AB, Stockholm
 */

#ifndef TWINE_WINDOWS_THREADING_H
#define TWINE_WINDOWS_THREADING_H
#ifdef TWINE_WINDOWS_THREADING

using pthread_t = int;
using pthread_attr_t = int;
using pthread_mutex_t = int;
using pthread_cond_t = int;
using sem_t = int;

#endif
#endif //TWINE_WINDOWS_THREADING_H
