/**
 * @file consumer.h
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

#pragma once
#ifndef NRS_EV_LOOP_BEANSTALKD_CONSUMER_H_
#define NRS_EV_LOOP_BEANSTALKD_CONSUMER_H_

#include "ev/loggable.h"

#include "ev/beanstalk/consumer.h"

#include "json/json.h"

#include <functional> // std::function

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Consumer
            {
                
            public: // Data Type(s)
                
                typedef std::function<void(const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code)> SuccessCallback;
                typedef std::function<void()>                                                                                  CancelledCallback;
                
            public: // Constructor(s) / Destructor
                
                Consumer ();
                virtual ~Consumer ();
                
            public: // Pure Virtual Method(s) / Function(s)
                
                virtual void Consume (const int64_t& a_id, const Json::Value& a_payload,
                                      const SuccessCallback& a_success_callback, const CancelledCallback& a_cancelled_callback) = 0;

            }; // end of class 'Consumer'
            
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_CONSUMER_H_

