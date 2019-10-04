/**
 * @file logger_v2.cc
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ev/logger_v2.h"

char*  ev::LoggerV2::s_buffer_          = nullptr;
size_t ev::LoggerV2::s_buffer_capacity_ = 0;
uid_t  ev::LoggerV2::s_user_id_         = UINT32_MAX;
gid_t  ev::LoggerV2::s_group_id_        = UINT32_MAX;

/**
 * @brief Calculate the number of digits for the provided value.
 *
 * @param a_value Value to be used.
 * 
 * @return Number of digits for provided value.
 */
size_t ev::LoggerV2::NumberOfDigits (size_t a_value)
{
    size_t count = 0;
    while ( 0 != a_value ) {
        a_value /= 10;
        count++;
    }
    return count;
}
