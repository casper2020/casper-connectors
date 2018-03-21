/**
 * @file error.cc
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

#include "ev/redis/error.h"

#include <vector>     // std::vector
#include <cstdarg>    // va_start, va_end, std::va_list
#include <cstddef>    // std::size_t
#include <stdexcept>  // std::runtime_error

/**
 * @brief Default constructor.
 *
 * @param a_message
 */
ev::redis::Error::Error (const std::string& a_message)
    : ::ev::Error(::ev::Object::Target::Redis, a_message)
{
    message_ = a_message;
}

/**
 * @brief VA constructor.
 *
 * @param a_format
 * @param ...
 */
ev::redis::Error::Error (const char* const a_format, ...)
    : ::ev::Error(::ev::Object::Target::Redis, "")
{
    
    auto temp   = std::vector<char> {};
    auto length = std::size_t { 512 };
    std::va_list args;
    while ( temp.size() <= length ) {
        temp.resize(length + 1);
        va_start(args, a_format);
        const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
        va_end(args);
        if ( status < 0 ) {
            throw std::runtime_error {"string formatting error"};
        }
        length = static_cast<std::size_t>(status);
    }
    message_ = length > 0 ? std::string { temp.data(), length } : "";
}

/**
 * @brief Destructor.
 */
ev::redis::Error::~Error ()
{
    /* empty */
}
