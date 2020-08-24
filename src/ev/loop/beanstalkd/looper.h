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
#include <mutex>
#include <queue>

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
                
            private: // Control Data
                
                typedef struct {
                    float timeout_; //!< in milleseconds
                    bool  set_;     //!< true if it was set, false otherwise.
                } Polling;
                
                Polling polling_;
                
            public: // Data Type(s)
                
                typedef std::function<void(const std::string&)> IdleCallback;
                
            private: // Threading
                
                typedef struct _IdleCallbackData {
                    
                    const std::string                     id_;
                    const size_t                          timeout_;
                    bool                                  recurrent_;
                    IdleCallback                          function_;
                    std::chrono::steady_clock::time_point end_tp_;
            
                } IdleCallbackData;
                
                struct IdleCallbackComparator{
                    bool operator()(const IdleCallbackData* a_lhs, const IdleCallbackData* a_rhs)
                    {
                        return a_lhs->end_tp_ > a_rhs->end_tp_;
                    }
                };
                
                typedef struct {
                    std::mutex                                                                                     mutex_;
                    std::priority_queue<IdleCallbackData*, std::vector<IdleCallbackData*>, IdleCallbackComparator> queue_;
                    std::set<std::string>                                                                          cancelled_;
                } Callbacks;
                
                Callbacks idle_callbacks_;
                
            public: // Constructor(s) / Destructor
                
                Looper (const ev::Loggable::Data& a_loggable_data,
                        const Job::Factory& a_factory, const Job::MessagePumpCallbacks& a_callbacks,
                        const std::string a_default_tube = "");
                virtual ~Looper ();
                
            public: // Method(s) / Function(s)
                
                void Run (const ::ev::beanstalk::Config& a_beanstakd_config,
                          const std::string& a_output_directory, const std::string& a_logs_directory, const std::string& a_shared_directory,
                          volatile bool& a_aborted);
                
                void AppendCallback (const std::string& a_id, IdleCallback a_callback,
                                     const size_t a_timeout = 0, const bool a_recurrent = false);
                
                void RemoveCallback (const std::string& a_id);
                
                
                void SetPollingTimeout (const float& a_millseconds);
                
            private: // Method(s) / Function(s)
                
                void Idle (const bool a_fake);
                
            }; // end of class 'Looper'
        
            /**
             * @brief Set polling timeout, < 0 will default to the one provided in config.
             *
             * @param a_millseconds Polling timeout in milliseconds,
             */
            inline void Looper::SetPollingTimeout (const float& a_millseconds)
            {
                polling_.timeout_ = a_millseconds;
                polling_.set_     = ( polling_.timeout_ > -1.0 );
            }
            
        } // end of namespace 'beanstalkd'
            
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_LOOPER_H_

