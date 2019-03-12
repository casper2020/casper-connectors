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

#include "ev/loggable.h"

#include "ev/beanstalk/consumer.h"

#include "ev/loop/beanstalkd/consumer.h"

#include <map>
#include <string>

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Looper final
            {
                
            public: // typedefs
                
                typedef std::map<std::string, std::function<Consumer*()>> Factories;
                typedef std::map<std::string, Consumer*>                  Consumers;
                
            private: // Const Refs
                
                const Factories&   factories_;
                
            private: // Const Data
                
                const std::string  default_tube_;
                
            private: // Data
                
                ::ev::beanstalk::Consumer* consumer_;
                Consumers                  consumers_;
                Consumer*                  consumer_ptr_;
                Json::Reader               json_reader_;
                Json::Value                job_payload_;
                
            public: // Constructor(s) / Destructor
                
                Looper (const Factories& a_factories, const std::string a_default_tube = "");
                virtual ~Looper ();
                
            public: // Method(s) / Function(s)
                
                void Run (ev::Loggable::Data& a_loggable_data, const ::ev::beanstalk::Config& a_beanstakd_config, volatile bool& a_aborted);
                
            }; // end of class 'Looper'
            
        } // end of namespace 'beanstalkd'
            
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_LOOPER_H_

