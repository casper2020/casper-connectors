/**
* @file tube.h
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors.
*
* casper-connectors is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-connectors  is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cc/job/easy/tube.h"

/**
 * @brief Default constructor.
 *
 * @param a_tube
 * @param a_config
 * @param a_loggable_data

 */
cc::job::easy::Tube::Tube (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config,
                           cc::job::easy::Runnable* a_runnable)
    : ev::loop::beanstalkd::Job(a_loggable_data, a_tube, a_config)
{
    runnable_ = a_runnable;
}

/**
 * @brief Destructor
 */
cc::job::easy::Tube::~Tube ()
{
    if ( nullptr != runnable_ ) {
        delete runnable_;
    }
}

/**
 * @brief Run a 'payroll' calculation.
 *
 * @param a_id                  Job ID
 * @param a_payload             Job Payload
 * @param a_callback            Success / Failure callback.
 * @param a_cancelled_callback
 */
void cc::job::easy::Tube::Run (const int64_t& a_id, const Json::Value& a_payload,
                               const cc::job::easy::Tube::CompletedCallback& a_completed_callback,
                               const cc::job::easy::Tube::CancelledCallback& a_cancelled_callback)
{

    runnable_->Run(a_id, a_payload);
    // TODO 2.0
//    
//    if ( true == WasCancelled() || true == AlreadyRan() ) {
//       a_cancelled_callback(AlreadyRan());
//   } else {
//       a_completed_callback(/* a_uri */ "", /* a_success */ false == HasErrorsSet(), /* a_http_status_code */ http_status_code_);
//   }
}
