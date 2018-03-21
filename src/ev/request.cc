/**
 * @file request.cc
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

#include "ev/request.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_target
 * @param a_mode
 * @param a_control
 */
ev::Request::Request (const ::ev::Loggable::Data& a_loggable_data,
                      const ev::Object::Target a_target, const ev::Request::Mode a_mode,
                      const ev::Request::Control a_control)
    : ev::Object(ev::Object::Type::Request, a_target),
      loggable_data_(a_loggable_data), mode_(a_mode), control_(a_control)
{
    invoke_id_        = 0;
    tag_              = 0;
    result_           = nullptr;
    start_time_point_ = std::chrono::steady_clock::now();
    timeout_in_ms_    = 0;
}

/**
 * @brief Destructor.
 */
ev::Request::~Request ()
{
    if ( nullptr != result_ ) {
        delete result_;
    }
}
