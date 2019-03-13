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

#include "ev/scheduler/scheduler.h"

#include "ev/loggable.h"

#include "ev/beanstalk/producer.h"

#include "ev/postgresql/json_api.h"

#include "json/json.h"


namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Job : private ev::scheduler::Scheduler::Client, private ev::redis::subscriptions::Manager::Client
            {
                
            public: // Data Type
                
                class Config
                {
                    
                public: // Const Data
                    
                    std::string       service_id_;
                    bool              transient_;
                    std::string       output_dir_;
                    
                public: // Constructor(s) / Destructor
                    
                    Config() = delete;
                    
                    Config (const std::string& a_service_id, const bool a_transient, const std::string& a_output_dir)
                    : service_id_(a_service_id), transient_(a_transient), output_dir_(a_output_dir)
                    {
                        /* empty */
                    }
                    
                    Config (const Config& a_config)
                    : service_id_(a_config.service_id_), transient_(a_config.transient_), output_dir_(a_config.output_dir_)
                    {
                        /* empty */
                    }
                    
                }; // end of class 'Config';
                
                typedef std::function<void(const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code)> CompletedCallback;
                typedef std::function<void()>                                                                                  CancelledCallback;

                typedef std::function<void(const ev::Exception&)>                                                              FatalExceptionCallback;
                typedef std::function<void(std::function<void()> a_callback, bool a_blocking)>                                 DispatchOnMainThread;
                typedef std::function<void(const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)>    SubmitJobCallback;
                
                typedef struct {
                    FatalExceptionCallback on_fatal_exception_;
                    DispatchOnMainThread   dispatch_on_main_thread_;
                    SubmitJobCallback      on_submit_job_;
                } MessagePumpCallbacks;
                
                typedef std::function<Job*(const std::string& a_tube)> Factory;
                
            protected: // Data Type(s)
                
                typedef struct {
                    const char* const  key_;
                    const Json::Value* args_;
                    uint8_t            value_;
                } Progress;
                
            protected: // Const Data
                
                const std::string         tube_;
                const Config              config_;
                
                const std::string         redis_signal_channel_;
                const std::string         redis_key_prefix_;
                const std::string         redis_channel_prefix_;
                
            protected: // Logs Data
                
                ev::Loggable::Data        loggable_data_;
                
            protected: // Data
                
                int64_t                   id_;
                std::string               channel_;
                int64_t                   validity_;
                bool                      transient_;
                bool                      cancelled_;
                Json::Value               response_;
                Json::Value               progress_;
                std::string               signal_channel_;
                
            private: // Data
                
                cc::UTCTime::HumanReadable chdir_hrt_;
                char                       hrt_buffer_[27];
                std::string                output_directory_;

            protected: // Helpers
                
                ::ev::postgresql::JSONAPI json_api_;
                Json::Reader              json_reader_;
                Json::FastWriter          json_writer_;
                Json::StyledWriter        json_styled_writer_;
                
            private: // Ptrs
                
                const MessagePumpCallbacks* callbacks_ptr_;
                
            public: // Constructor(s) / Destructor
                
                Job (const std::string& a_tube, const Config& a_config, const ev::Loggable::Data& a_loggable_data);
                virtual ~Job ();
                
            public: // Inline Method(s) / Function(s)
                
                bool IsCancelled  () const;
                
            public: // Method(s) / Function(s)
                
                void Setup   (const MessagePumpCallbacks* a_callbacks);
                void Consume (const int64_t& a_id, const Json::Value& a_payload,
                              const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback);
                
            protected: // Pure Virtual Method(s) / Function(s)
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback) = 0;
                
            protected: // Method(s) / Function(s)
                
                void ConfigJSONAPI (const Json::Value& a_config);
                
            protected: // REDIS Helper Method(s) / Function(s)
                
                void PublishCancelled ();
                void PublishFinished  (const Json::Value& a_payload);
                void PublishProgress  (const Json::Value& a_payload);
                void PublishProgress  (const Progress& a_message);
                
                void PublishSignal    (const Json::Value& a_object);
                
                void Publish (const std::string& a_channel, const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void Publish (const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void GetJobCancellationFlag ();
                
            protected: // Beanstalk Helper Method(s) / Function(s)
                
                void SubmitJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr);
                
            protected: // Threading Helper Methods(s) / Function(s)
                
                void ExecuteOnMainThread (std::function<void()> a_callback, bool a_blocking);
                void OnFatalException    (const ev::Exception& a_exception);
                
            protected: // PostgreSQL Helper Methods(s) / Function(s)
                
                virtual void ExecuteQuery            (const std::string& a_query, Json::Value& o_result,
                                                      const bool a_use_column_name);
                virtual void ExecuteQueryWithJSONAPI (const std::string& a_query, Json::Value& o_result);
                
            protected: // Output Helper Methods(s) / Function(s)

                const std::string& EnsureOutputDir (const int64_t a_validity);
                
            protected: // JsonCPP Helper Methods(s) / Function(s)
                
                Json::Value GetJSONObject (const Json::Value& a_parent, const char* const a_key,
                                           const Json::ValueType& a_type, const Json::Value* a_default);
                
            protected: //
                
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



