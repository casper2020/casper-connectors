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
#ifndef NRS_EV_LOOP_BEANSTALKD_LOOPER_H_
#define NRS_EV_LOOP_BEANSTALKD_LOOPER_H_

#include "ev/loop/beanstalkd/object.h"

#include "ev/loggable.h"

#include "ev/exception.h"

#include "ev/loop/beanstalkd/job.h"

#include "ev/beanstalk/consumer.h"

#include <map>
#include <string>

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Looper final : private ::ev::loop::beanstalkd::Object
            {
                
            private: // Data Type(s)
                
                typedef std::map<std::string, Job*> Cache;
                
            private: // Const Refs
                
                const Job::Factory&              factory_;
                const Job::MessagePumpCallbacks& callbacks_;
                
            private: // Const Data
                
                const std::string                 default_tube_;
                
            private: // Data
                
                ::ev::beanstalk::Consumer* beanstalk_;
                Cache                      cache_;
                Job*                       job_ptr_;
                Json::Reader               json_reader_;
                Json::Value                job_payload_;
                
            public: // Constructor(s) / Destructor
                
                Looper (const ev::Loggable::Data& a_loggable_data,
                        const Job::Factory& a_factory, const Job::MessagePumpCallbacks& a_callbacks,
                        const std::string a_default_tube = "");
                virtual ~Looper ();
                
            public: // Method(s) / Function(s)
                
                void Run (const ::ev::beanstalk::Config& a_beanstakd_config, const std::string& a_output_directory,
                          volatile bool& a_aborted);
                
            }; // end of class 'Looper'
            
        } // end of namespace 'beanstalkd'
            
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_LOOPER_H_

