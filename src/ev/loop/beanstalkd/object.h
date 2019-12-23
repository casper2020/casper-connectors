/**
 * @file object.h
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
#ifndef NRS_EV_LOOP_BEANSTALKD_OBJECT_H_
#define NRS_EV_LOOP_BEANSTALKD_OBJECT_H_

#include "ev/logger_v2.h"

#if 1

#define EV_LOOP_BEANSTALK_IF_LOG_ENABLED(a_code) \
    a_code

#define EV_LOOP_BEANSTALK_LOG(a_token, a_format, ...) \
    ev::LoggerV2::GetInstance().Log(logger_client_, a_token, \
        a_format, __VA_ARGS__ \
    );

#endif

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
        
            class Object
            {
                
            protected: // Data
                
                ::ev::LoggerV2::Client* logger_client_;
                
            protected: // Data
                
                ev::Loggable::Data loggable_data_;
                
            public: // Constructor(s) / Destructor

                Object() = delete;
                Object (const ev::Loggable::Data& a_loggable_data);
                virtual ~Object();

            };
            
        } // end of namespace 'beanstalkd'
    
    } // end of namespace 'loop'

} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_OBJECT_H_
