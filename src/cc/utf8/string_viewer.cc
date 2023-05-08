/**
 * @file string_viewer.cc
 *
 * Based on code originally developed for NDrive S.A.
 *
 * Copyright (c) 2011-2023 Cloudware S.A. All rights reserved.
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
 * along with osal.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/utf8/string_viewer.h"

#include "cc/utf8/collation_tables.h"

#include <tuple> // make_tuple

/**
 * @brief C string constructor.
 */
cc::utf8::StringViewer::StringViewer (const char* a_string)
    : start_ptr_(a_string)
{
    current_ptr_  = start_ptr_;
    current_char_ = 0;
}

/**
 * @brief Standard C++ string.
 */
cc::utf8::StringViewer::StringViewer (const std::string& a_string)
    : start_ptr_(a_string.c_str())
{
    current_ptr_  = start_ptr_;
    current_char_ = 0;
}

/**
 * @brief Destructor
 */
cc::utf8::StringViewer::~StringViewer ()
{
    /* empty */
}

/**
 * @brief Count the number of utf8 chars in a string.
 *
 * @param a_string
 *
 * @return The number of UTF8 chars.
 */
size_t cc::utf8::StringViewer::CharsCount (const std::string& a_string)
{
    cc::utf8::StringViewer it = cc::utf8::StringViewer(a_string);

    size_t count = 0;
    while ( 0 != it.Next() ) {
        count++;
    }
    return count;
}

/**
 * @brief Count the number of UTF8 chars and bytes in a string.
 *
 * @param a_string
 *
 * @return The number of UTF8 chars and bytes in a string.
 */
std::tuple<size_t, size_t> cc::utf8::StringViewer::Count (const std::string& a_string)
{
    return std::make_tuple(a_string.length() > 0 ? cc::utf8::StringViewer::CharsCount(a_string) : 0, a_string.length());
}
