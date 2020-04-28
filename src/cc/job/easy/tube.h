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
#pragma once
#ifndef NRS_CC_EASY_JOB_TUBE_H_
#define NRS_CC_EASY_JOB_TUBE_H_

#include "ev/loop/beanstalkd/job.h"

#include "cc/job/easy/runnable.h"

namespace cc
{

    namespace job
    {

        namespace easy
        {
        
            class Tube : public ev::loop::beanstalkd::Job
            {
                
            public: // Data Type(s)
                
                typedef ev::loop::beanstalkd::Job::Config Config;
                
            protected: //
                
                cc::job::easy::Runnable* runnable_; // ONWER WILL BE THIS CLASS
                
            public: // Constructor(s) / Destructor
                
                Tube (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config,
                      cc::job::easy::Runnable* a_runnable);
                virtual ~Tube ();
                
            protected: // Inherited Virtual Method(s) / Function(s) - from ev::loop::beanstalkd::Job::
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback);

                
            }; // end of class 'Tube'
            
        } // end of namespace 'easy'
    
    } // end of namespace 'job'

} // end of namespace 'cc'
        
#endif // NRS_CC_EASY_JOB_TUBE_H_
