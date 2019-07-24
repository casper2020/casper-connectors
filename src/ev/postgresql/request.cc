/**
 * @file request.cc - PostgreSQL
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

#include "ev/postgresql/request.h"

#include <vector>  // std::vector
#include <cstdarg> // va_start, va_end, std::va_list

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data object for logging
 * @param a_payload char array vector with query
 */
ev::postgresql::Request::Request (const ::ev::Loggable::Data& a_loggable_data, const std::vector<char>& a_payload)
: ev::Request(a_loggable_data, ev::Object::Target::PostgreSQL, ev::Request::Mode::OneShot)
{
    payload_ = a_payload.data();
}

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_payload
 */
ev::postgresql::Request::Request (const ::ev::Loggable::Data& a_loggable_data, const std::string& a_payload)
    : ev::Request(a_loggable_data, ev::Object::Target::PostgreSQL, ev::Request::Mode::OneShot)
{
    payload_ = a_payload;
}

/**
 * @brief VA constructor.
 *
 * @param a_loggable_data
 * @param a_format
 * @param ...
 */
ev::postgresql::Request::Request (const ::ev::Loggable::Data& a_loggable_data, const char* const a_format, ...)
    : ev::Request(a_loggable_data, ev::Object::Target::PostgreSQL, ev::Request::Mode::OneShot)
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
    payload_ = length > 0 ? std::string { temp.data(), length } : "";
}

/**
 * @brief va_list constructor.
 *
 * @param a_loggable_data
 * @param a_size
 * @param a_format
 * @param a_list
 */
ev::postgresql::Request::Request (const ::ev::Loggable::Data& a_loggable_data, size_t a_size, const char* const a_format, va_list a_list)
: ev::Request(a_loggable_data, ev::Object::Target::PostgreSQL, ev::Request::Mode::OneShot)
{
    va_list args;
    size_t  size   = a_size;
    
    char* buffer = (char*)malloc(size);
    if ( nullptr == buffer ) {
        throw ::ev::Exception("Out of memory!");
    }

    va_copy(args, a_list);
    int written = vsnprintf(buffer, size, a_format, args);
    va_end(args);
    
    if ( written < 0 ) {
        throw ::ev::Exception("string formatting error!");
    } else if ( written >= size ) {
        size   = static_cast<size_t>(written + sizeof(char));
        buffer = (char*) realloc(buffer, size );
        if ( nullptr == buffer ) {
            throw ::ev::Exception("Out of memory while reallocating buffer!");
        }
        
        va_copy(args, a_list);
        written = vsnprintf(buffer, size, a_format, args);
        if ( written != static_cast<int>(size - sizeof(char)) ) {
            throw ::ev::Exception("String formatting error or buffer not large enough!");
        }
        va_end(args);
    }
    payload_ = std::string(buffer);
    free(buffer);
}

/**
 * @brief VA constructor.
 *
 * @param a_loggable_data
 * @param a_format
 * @param ...
 */
ev::postgresql::Request::Request (const char* a_query, const ::ev::Loggable::Data& a_loggable_data)
: ev::Request(a_loggable_data, ev::Object::Target::PostgreSQL, ev::Request::Mode::OneShot)
{
    payload_ = a_query;
}

/**
 * @brief Destructor
 */
ev::postgresql::Request::~Request ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @return This object C string representation.
 */
const char* ev::postgresql::Request::AsCString () const
{
    return payload_.c_str();
}

/**
 * @return This object string representation.
 */
const std::string& ev::postgresql::Request::AsString () const
{
    return payload_;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Escape an SQL term.
 *
 * @param a_value
 * @param o_value
 */
void ev::postgresql::Request::SQLEscape (const std::string& a_value, std::string& o_value)
{
    o_value = "";
    const size_t count = a_value.size();
    if ( 0 == count ) {
        return;
    }
    for ( size_t idx = 0 ; idx < a_value.size(); ++idx ) {
        if ( '\'' == a_value[idx] ) {
            o_value += "''";
        } else {
            o_value += a_value[idx];
        }
    }
}

/**
 * @brief Escape an SQL term.
 *
 * @param a_value
 *
 * @return
 */
std::string ev::postgresql::Request::SQLEscape (const std::string& a_value)
{
    std::string value = "";
    const size_t count = a_value.size();
    if ( 0 == count ) {
        return "";
    }
    for ( size_t idx = 0 ; idx < a_value.size(); ++idx ) {
        if ( '\'' == a_value[idx] ) {
            value += "''";
        } else {
            value += a_value[idx];
        }
    }
    return value;
}
