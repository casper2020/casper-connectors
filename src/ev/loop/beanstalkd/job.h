/**
 * @file job.h
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
#ifndef NRS_EV_LOOP_BEANSTALKD_JOB_H_
#define NRS_EV_LOOP_BEANSTALKD_JOB_H_

#include "ev/redis/subscriptions/manager.h"

#include "ev/loop/beanstalkd/consumer.h"

#include "ev/scheduler/scheduler.h"

#include "ev/loggable.h"

#include "ev/postgresql/json_api.h"

#include "json/json.h"


namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Job : public ev::loop::beanstalkd::Consumer, private ev::scheduler::Scheduler::Client, private ev::redis::subscriptions::Manager::Client
            {
                
            public: // Data Type
                
                class Config
                {
                    
                public: // Data Type(s)
                    
                    typedef std::function<void(const ev::Exception& a_ev_exception)> FatalExceptionCallback;
                    
                public: // Const Data
                    
                    const std::string             service_id_;
                    const std::string             tube_;
                    const bool                    transient_;
                    const std::string             logs_dir_;
                    const ev::Loggable::Data&     loggable_data_ref_;
                    const FatalExceptionCallback& fatal_exception_callback_;
                    
                public: // Constructor(s) / Destructor
                    
                    Config() = delete;
                    
                    Config (const std::string& a_service_id, const std::string& a_tube, const bool a_transient,
                            const std::string& a_logs_dir, const ev::Loggable::Data& a_loggable_data_ref,
                            const FatalExceptionCallback& a_fatal_exception_callback)
                    : service_id_(a_service_id), tube_(a_tube), transient_(a_transient),
                      logs_dir_(a_logs_dir), loggable_data_ref_(a_loggable_data_ref), fatal_exception_callback_(a_fatal_exception_callback)
                    {
                        /* empty */
                    }
                    
                    Config (const Config& a_config)
                    : service_id_(a_config.service_id_), tube_(a_config.tube_), transient_(a_config.transient_),
                      logs_dir_(a_config.logs_dir_), loggable_data_ref_(a_config.loggable_data_ref_),
                      fatal_exception_callback_(a_config.fatal_exception_callback_)
                    {
                        /* empty */
                    }
                    
                }; // end class 'Config';
                
            protected: // Data Type(s)
                
                typedef struct {
                    const char* const  key_;
                    const Json::Value* args_;
                    uint8_t            value_;
                } Progress;
                
            protected: // Const Data
                
                const Config              config_;
                
                const std::string         redis_signal_channel_;
                const std::string         redis_key_prefix_;
                const std::string         redis_channel_prefix_;
                
            protected: // Data
                
                int64_t                   id_;
                std::string               channel_;
                int64_t                   validity_;
                bool                      transient_;
                bool                      cancelled_;
                Json::Value               response_;
                Json::Value               progress_;
                
            protected: // Helpers
                
                ::ev::postgresql::JSONAPI json_api_;
                Json::Reader              json_reader_;
                Json::FastWriter          json_writer_;
                
            public: // Constructor(s) / Destructor
                
                Job (const Config& a_config);
                virtual ~Job ();
                
            public: // Inline Method(s) / Function(s)
                
                bool IsCancelled() const;
                
            public: // Inherited Virtual Method(s) / Function(s) - from beanstalkd::Consumer
                
                virtual void Consume (const int64_t& a_id, const Json::Value& a_payload,
                                      const SuccessCallback& a_success_callback, const CancelledCallback& a_cancelled_callback);
                
            protected: // Pure Virtual Method(s) / Function(s)
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const SuccessCallback& a_success_callback, const CancelledCallback& a_cancelled_callback) = 0;
                
            protected: // Method(s) / Function(s)
                
                void ConfigJSONAPI (const Json::Value& a_config);
                
            protected: // REDIS Helper Method(s) / Function(s)
                
                void PublishCancelled ();
                void PublishFinished  (const Json::Value& a_payload);
                void PublishProgress  (const Json::Value& a_payload);
                void PublishProgress  (const Progress& a_message);
                
                void Publish (const std::string& a_channel, const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void Publish (const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void GetJobCancellationFlag ();
                
            protected: // PostgreSQL Helper Methods(s) / Function(s)
                
                virtual void ExecuteQuery            (const std::string& a_query, Json::Value& o_result,
                                                      const bool a_use_column_name);
                virtual void ExecuteQueryWithJSONAPI (const std::string& a_query, Json::Value& o_result);
                
            protected: // JsonCPP Helper Methods(s) / Function(s)
                
                Json::Value GetJSONObject (const Json::Value& a_parent, const char* const a_key,
                                           const Json::ValueType& a_type, const Json::Value* a_default);
                
            private: //
                
                ev::scheduler::Task*                             NewTask                (const EV_TASK_PARAMS& a_callback);
                EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK JobSignalsDataCallback (const std::string& a_name, const std::string& a_message);
                
            private: // from ::ev::redis::SubscriptionsManager::Client
                
                virtual void OnREDISConnectionLost ();
                
            }; // end of class 'Job'
            
            /**
             * @return True if the job was cancelled, false otherwise.
             */
            inline bool Job::IsCancelled() const
            {
                return cancelled_;
            }
            
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_JOB_H_



