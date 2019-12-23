/**
* @file object.cc
*
* Copyright (c) 2010-2017 Cloudware S.A. All rights reserved.
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

#include "ev/loop/beanstalkd/object.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 */
ev::loop::beanstalkd::Object::Object (const ev::Loggable::Data& a_loggable_data)
    : loggable_data_(a_loggable_data)
{
    logger_client_ = new ::ev::LoggerV2::Client(loggable_data_);
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Object::~Object ()
{
    ev::LoggerV2::GetInstance().Unregister(logger_client_);
    delete logger_client_;
}
