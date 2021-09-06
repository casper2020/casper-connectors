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
#ifndef NRS_CC_EASY_JOB_H_
#define NRS_CC_EASY_JOB_H_

#include "ev/loop/beanstalkd/job.h"

#include "cc/easy/job/types.h"

#include "cc/debug/types.h"

#include "ev/logger_v2.h"

#ifdef CC_LOGS_LOGGER_COLOR
    #define CC_JOB_LOG_COLOR(a_name) CC_LOGS_LOGGER_COLOR(a_name)
#else
    #define CC_JOB_LOG_COLOR(a_name) ""
#endif

#define CC_JOB_LOG_ENABLE(a_tube, a_uri) \
    ::ev::LoggerV2::GetInstance().cc::logs::Logger::Register(a_tube, a_uri);

#define CC_JOB_LOG_REGISTER(a_token) do { \
    if ( false == ::ev::LoggerV2::GetInstance().IsRegistered(logger_client_, a_token.c_str()) ) { \
        ::ev::LoggerV2::GetInstance().Register(logger_client_, { a_token.c_str() }); \
    } \
} while(0)

#define CC_JOB_LOG_UNREGISTER() \
    ::ev::LoggerV2::GetInstance().Unregister(logger_client_);

#define CC_JOB_LOG(a_level, a_id, a_format, ...) \
    if ( a_level <= log_level_ ) \
        ::ev::LoggerV2::GetInstance().Log(logger_client_, tube_.c_str(), "Job #" INT64_FMT ", " a_format, a_id, __VA_ARGS__)

#define CC_JOB_LOG_TRACE(a_level, a_format, ...) \
    if ( a_level <= log_level_ ) \
        ::ev::LoggerV2::GetInstance().Log(logger_client_, tube_.c_str(), "\n[%s] @ %-4s:%4d\n\n\t* " a_format "\n",  tube_.c_str(), __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)

#define CC_JOB_LOG_STATIC(a_id, a_client, a_tube, a_format, ...) \
    ev::LoggerV2::GetInstance().Log(a_client, \
        a_tube.c_str(), "Job #" INT64_FMT ", " a_format, a_id, __VA_ARGS__ \
    );


namespace cc
{

    namespace easy
    {

        namespace job
        {
        
            class Job : public ev::loop::beanstalkd::Job
            {
            
            protected: // Const Data
                
                CC_IF_DEBUG_DECLARE_VAR(const cc::debug::Threading::ThreadID, thread_id_);
                
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
                
            protected: // Virtual Method(s) / Function(s)
                
                virtual void LogResponse (const Response& /* a_response */, const Json::Value& /* a_payload */) {};
                
            protected: // Method(s) / Function(s)

                // 1 ~ 5 xxx
                uint16_t SetI18NMessage                (const uint16_t& a_code, const I18N& a_i18n, Json::Value& o_payload);
                uint16_t SetI18NError                  (const uint16_t& a_code, const I18N& a_i18n, const InternalError& a_error,
                                                        Json::Value& o_payload);
                // 2xx
                uint16_t SetOk                         (const I18N* a_i18n, Json::Value& o_payload);
                
                // ! 2xxx
                uint16_t SetError                       (const uint16_t& a_code, const I18N* a_i18n, const easy::job::InternalError& a_error, Json::Value& o_payload);
                
                // 4xx
                uint16_t SetBadRequest                  (const I18N* a_i18n, const InternalError& a_error, Json::Value& o_payload);
                uint16_t SetBadRequest                  (const I18N* a_i18n, Json::Value& o_payload);
                uint16_t SetForbidden                   (const I18N* a_i18n, Json::Value& o_payload);
                uint16_t SetForbidden                   (const I18N* a_i18n, const InternalError& a_error, Json::Value& o_payload);
                uint16_t SetNotFound                    (const I18N* a_i18n, Json::Value& o_payload);
                uint16_t SetNotFound                    (const I18N* a_i18n, const InternalError& a_error, Json::Value& o_payload);
                uint16_t SetNotAcceptable               (const I18N* a_i18n, Json::Value& o_payload);
                uint16_t SetNotAcceptable               (const I18N* a_i18n, const InternalError& a_error, Json::Value& o_payload);
                uint16_t SetTimeout                     (const I18N* a_i18n, Json::Value& o_payload);

                // 5xx
                uint16_t SetInternalServerError         (const I18N* a_i18n, Json::Value& o_payload);
                uint16_t SetInternalServerError         (const I18N* a_i18n, const InternalError& a_error        , Json::Value& o_payload);
                uint16_t SetInternalServerError         (const I18N* a_i18n, const InternalException& a_exception, Json::Value& o_payload);

                uint16_t SetNotImplemented              (const I18N* a_i18n, Json::Value& o_payload);
                uint16_t SetNotImplemented              (const I18N* a_i18n, const InternalError& a_error, Json::Value& o_payload);

            }; // end of class 'Job'
            
        } // end of namespace 'job'
    
    } // end of namespace 'easy'

} // end of namespace 'cc'
        
#endif // NRS_CC_EASY_JOB_H_
