/**
 * @file request.cc - REDIS
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

#include "ev/redis/request.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_command
 * @param a_args
 */
ev::redis::Request::Request (const ::ev::Loggable::Data& a_loggable_data, const char* const a_command, const std::vector<std::string>& a_args)
    : ev::Request(a_loggable_data, ev::Object::Target::Redis, ev::Request::Mode::OneShot),
      kind_(ev::redis::Request::Kind::Other)
{
    SetPayload(a_command, a_args);
}

/**
 * @brief Constructor.
 *
 * @param a_loggable_data
 * @param a_mode
 * @param a_kind
 */
ev::redis::Request::Request (const ::ev::Loggable::Data& a_loggable_data, const ev::Request::Mode a_mode, const ev::redis::Request::Kind a_kind)
    : ev::Request(a_loggable_data, ev::Object::Target::Redis, a_mode),
      kind_(a_kind)
{
    /* empty */
}

/**
 * @brief Destructor
 */
ev::redis::Request::~Request ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @return This object C string representation.
 */
const char* const ev::redis::Request::AsCString () const
{
    return payload_.c_str();
}

/**
 * @return This object string representation.
 */
const std::string& ev::redis::Request::AsString () const
{
    return payload_;
}
