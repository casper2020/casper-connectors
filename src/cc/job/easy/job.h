/**
 * @file job.h
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
#ifndef NRS_CC_JOB_EASY_JOB_H_
#define NRS_CC_JOB_EASY_JOB_H_

#include "ev/loop/beanstalkd/job.h"

#include "cc/debug/types.h"

#include "ev/logger_v2.h"

#ifdef CC_LOGS_LOGGER_COLOR
    #define CC_JOB_LOG_COLOR(a_name) CC_LOGS_LOGGER_COLOR(a_name)
#else
    #define CC_JOB_LOG_COLOR(a_name) ""
#endif

#define CC_JOB_LOG_ENABLE(a_tube, a_uri) \
    ::ev::LoggerV2::GetInstance().cc::logs::Logger::Register(a_tube, a_uri);

#define CC_JOB_LOG_REGISTER() \
    if ( true == ::ev::LoggerV2::GetInstance().cc::logs::Logger::IsRegistered(tube_.c_str()) ) { \
        ::ev::LoggerV2::GetInstance().Register(logger_client_, { tube_.c_str() }); \
    }

#define CC_JOB_LOG_UNREGISTER() \
    if ( true == ::ev::LoggerV2::GetInstance().cc::logs::Logger::IsRegistered(tube_.c_str()) ) { \
        ::ev::LoggerV2::GetInstance().Unregister(logger_client_); \
    }

#define CC_JOB_LOG_LEVEL_CRT 1 // CRITICAL
#define CC_JOB_LOG_LEVEL_ERR 2 // ERROR
#define CC_JOB_LOG_LEVEL_WRN 3 // WARNING
#define CC_JOB_LOG_LEVEL_INF 4 // MESSAGE
#define CC_JOB_LOG_LEVEL_VBS 5 // VERBOSE
#define CC_JOB_LOG_LEVEL_DBG 6 // DEBUG

//                                   ------
#define CC_JOB_LOG_STEP_IN          "IN"
#define CC_JOB_LOG_STEP_OUT         "OUT"
#define CC_JOB_LOG_STEP_REDIS       "REDIS"
#define CC_JOB_LOG_STEP_POSGRESQL   "PGSQL"
#define CC_JOB_LOG_STEP_HTTP        "HTTP"
#define CC_JOB_LOG_STEP_BEANSTALK   "BT"
#define CC_JOB_LOG_STEP_STEP        "STEP"
#define CC_JOB_LOG_STEP_STATUS      "STATUS"
#define CC_JOB_LOG_STEP_STATS       "STATS"
#define CC_JOB_LOG_STEP_RELAY       "RELAY"
#define CC_JOB_LOG_STEP_RTT         "RTT"
#define CC_JOB_LOG_STEP_ERROR       "ERROR"
#define CC_JOB_LOG_STEP_V8          "V8"
#define CC_JOB_LOG_STEP_DUMP        "DUMP"

#define CC_JOB_LOG(a_level, a_id, a_format, ...) \
    if ( a_level <= log_level_ ) \
        ::ev::LoggerV2::GetInstance().Log(logger_client_, tube_.c_str(), "Job #" INT64_FMT ", " a_format, a_id, __VA_ARGS__)

#define CC_JOB_LOG_TRACE(a_level, a_format, ...) \
    if ( a_level <= log_level_ ) \
        ::ev::LoggerV2::GetInstance().Log(logger_client_, tube_.c_str(), "\n[%s] @ %-4s:%4d\n\n\t* " a_format "\n",  tube_.c_str(), __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)

namespace cc
{

    namespace job
    {

        namespace easy
        {
        
            class Job : public ev::loop::beanstalkd::Job
            {
                
            public: // Data Type(s)
                
                typedef ev::loop::beanstalkd::Job::Config Config;
                
                typedef struct {
                    uint16_t    code_;
                    Json::Value payload_;
                } Response;
                
            protected:
                
                size_t log_level_;
                
            public: // Constructor(s) / Destructor
                
                Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config);
                virtual ~Job ();
                
            protected: // Inherited Virtual Method(s) / Function(s) - from ev::loop::beanstalkd::Job::
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback, const DeferredCallback& a_deferred_callback);

            protected: // Pure Virtual Method(s) / Function(s)
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload, Response& o_response) = 0;
                
            }; // end of class 'Job'
            
        } // end of namespace 'easy'
    
    } // end of namespace 'job'

} // end of namespace 'cc'
        
#endif // NRS_CC_JOB_EASY_JOB_H_
