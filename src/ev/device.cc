/**
 * @file device.c
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

#include "ev/device.h"

#include "ev/logger.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 */
ev::Device::Device (const ::ev::Loggable::Data& a_loggable_data)
    : loggable_data_(a_loggable_data)
{
    last_error_code_   = -1;
    listener_ptr_      = nullptr;
    handler_ptr_       = nullptr;
    event_base_ptr_    = nullptr;
    connection_status_ = ev::Device::ConnectionStatus::Disconnected;
    reuse_count_       = 0;
    max_reuse_count_   = -1;
    tracked_           = true;
    invalidate_reuse_  = false;
}

/**
 * @brief Destructor.
 */
ev::Device::~Device ()
{
    // ... don't dealloc listener or handler, mem not managed by this class ...
    listener_ptr_ = nullptr;
    handler_ptr_  = nullptr;
}

/**
 * @brief Setup a device.
 *
 * @param a_event
 * @param a_exception_callback
 */
void ev::Device::Setup (struct event_base* a_event, ev::Device::ExceptionCallback a_exception_callback)
{
    event_base_ptr_     = a_event;
    exception_callback_ = a_exception_callback;
}
