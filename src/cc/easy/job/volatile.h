/**
 * @file volatile.h
 *
 * Copyright (c) 2011-2023 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_EASY_JOB_VOLATILE_H_
#define NRS_CC_EASY_JOB_VOLATILE_H_

#include "cc/non-movable.h"
#include "cc/non-copyable.h"

#include "ev/scheduler/scheduler.h"

#include "ev/beanstalk/producer.h"

#include "json/json.h"

#include <map>

namespace cc
{

    namespace easy
    {

        namespace job
        {
            
            class Volatile final : public ::cc::NonMovable, public ::cc::NonCopyable, private ::ev::scheduler::Client
            {
                
            public: // Data Type(s)
                
                typedef struct {
                    const std::string  service_id_;
                    const std::string  tube_;
                    const Json::Value& payload_;
                    const uint32_t     ttr_;
                    const uint32_t     validity_;
                    const int64_t      expires_in_;
                } Job;
                
                typedef struct {
                    uint64_t           id_;
                    std::string        key_;
                    std::string        channel_;
                    uint16_t           sc_;
                    std::string        ew_; // exception 'what'
                } Result;
                
            private: // Const Data
                
                const ev::beanstalk::Config config_;

            private: // Const Refs
                
                const ev::Loggable::Data& loggable_data_;
                                
            private: // Helper(s)
                
                std::map<std::string, ::ev::beanstalk::Producer*> producers_;
                
            public: // Constructor(s) / Destructor
                
                Volatile () = delete;
                Volatile (const ev::beanstalk::Config& a_config, const ev::Loggable::Data& a_loggable_data);
                virtual ~Volatile ();

            public: // One-shot Call Method(s) / Function(s)
                
                void Setup ();
                
            public: // Method(s) / Function(s)
                
                void Submit (const Job& a_job, Result& o_result, osal::ConditionVariable& a_cv);

            private: // Method(s) / Function(s)
                
                void ReserveAndPush (const Job& a_job, Result& o_result, osal::ConditionVariable& a_cv);
                
            private: // Method(s) / Function(s)
                
                ::ev::scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);
                
            }; // end of class 'Volatile'
            
        } // end of namespace 'job'

    } // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_JOB_VOLATILE_H_
