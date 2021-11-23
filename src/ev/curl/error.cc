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

#include "ev/curl/error.h"

#include <vector>     // std::vector
#include <cstdarg>    // va_start, va_end, std::va_list
#include <cstddef>    // std::size_t
#include <stdexcept>  // std::runtime_error

/**
 * @brief Constructor with error message.
 *
 * @param a_message
 */
ev::curl::Error::Error (const std::string& a_message)
    : ::ev::Error(::ev::Object::Target::CURL, a_message), code_(CURLcode::CURL_LAST)
{
    /* empty */
}

/**
 * @brief Constructor with error code and message.
 *
 * @param a_code
 * @param a_message
 */
ev::curl::Error::Error (const CURLcode a_code, const std::string& a_message)
: ::ev::Error(::ev::Object::Target::CURL, a_message), code_(a_code)
{
    /* empty */
}

/**
 * @brief VA constructor.
 *
 * @param a_format
 * @param ...
 */
ev::curl::Error::Error (const char* const a_format, ...)
    : ::ev::Error(::ev::Object::Target::CURL, ""),  code_(CURLcode::CURL_LAST)
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
    message_ = length > 0 ? std::string { temp.data(), length } : 0;
}

/**
 * @brief Destructor.
 */
ev::curl::Error::~Error ()
{
    /* empty */
}
