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

#include <limits>
#include <time.h>

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/redis/subscriptions/manager.h"

#include "ev/scheduler/scheduler.h"

#include "ev/loggable.h"

#include "ev/beanstalk/producer.h"

#include "ev/curl/http.h"
#include "ev/postgresql/json_api.h"

#include "json/json.h"

#include "ev/logger.h"
#define EV_LOOP_BEANSTALK_JOB_LOG_DEFINE_CONST(a_type, a_name, a_value) \
    a_type a_name = a_value

#if 1
    #define EV_LOOP_BEANSTALK_JOB_LOG(a_token, a_format, ...) \
        ev::Logger::GetInstance().Log(a_token, loggable_data_, \
            "Job #" INT64_FMT_MAX_RA " ~= " a_format, \
            ID(), __VA_ARGS__ \
        );
    #define EV_LOOP_BEANSTALK_JOB_LOG_JS(a_loggable_data, a_token, a_format, ...) \
        ev::Logger::GetInstance().Log(a_token, a_loggable_data, \
            "Job #" INT64_FMT_MAX_RA " ~= " a_format, \
            __VA_ARGS__ \
        );


#else
    #define EV_LOOP_BEANSTALK_JOB_LOG(a_token, a_format, ...) \
        ev::Logger::GetInstance().Log(a_token, loggable_data_, \
            "Job #" INT64_FMT_MAX_RA " ~= [ %6" PRIu64 " ms ] " a_format, \
            ID(), static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_tp_).count()), __VA_ARGS__ \
        );
#endif

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Job : private ev::scheduler::Scheduler::Client, private ev::redis::subscriptions::Manager::Client, public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            public: // Data Type
                
                class Config : public ::cc::NonMovable
                {
                    
                public: // Const Data
                    
                    std::string service_id_;
                    bool        transient_;
                    int         min_progress_;
                    
                public: // Constructor(s) / Destructor
                    
                    Config () = delete;
                    
                    Config (const std::string& a_service_id, const bool a_transient, const int a_min_progress = 3)
                        : service_id_(a_service_id), transient_(a_transient), min_progress_(a_min_progress)
                    {
                        /* empty */
                    }
                    
                    Config (const Config& a_config)
                        : service_id_(a_config.service_id_), transient_(a_config.transient_), min_progress_(a_config.min_progress_)
                    {
                        /* empty */
                    }
                    
                    inline Config& operator=(const Config& a_config)
                    {
                        service_id_ = a_config.service_id_;
                        transient_  = a_config.transient_;
                        min_progress_ = a_config.min_progress_;
                        return *this;
                    }
                    
                }; // end of class 'Config';
                
                typedef std::function<void(const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code)> CompletedCallback;
                typedef std::function<void()>                                                                                  CancelledCallback;

                typedef std::function<void(const ev::Exception&)>                                                              FatalExceptionCallback;
                typedef std::function<void(std::function<void()> a_callback, bool a_blocking)>                                 DispatchOnMainThread;
                typedef std::function<void(const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)>    SubmitJobCallback;
                
                typedef struct {
                    FatalExceptionCallback on_fatal_exception_;
                    DispatchOnMainThread   on_main_thread_;
                    SubmitJobCallback      on_submit_job_;
                } MessagePumpCallbacks;
                
                typedef std::function<Job*(const std::string& a_tube)> Factory;
                
            protected: // Data Type(s)
                
                enum class Status : uint8_t {
                    InProgress = 0,
                    Finished,
                    Failed,
                    Cancelled,
                };
                
                typedef struct {
                    const char* const                         key_;
                    const std::map<std::string, Json::Value>& args_;
                    Status                                    status_;
                    double                                    value_;
                    bool                                      now_;
                } Progress;
                
                typedef struct {
                    int                                   timeout_in_sec_;
                    std::chrono::steady_clock::time_point last_tp_;
                } ProgressReport;
                
            protected: // Const Static Data
                
                constexpr static const int k_exception_rc_ = -3;

            protected: // Const Data
                
                const std::string         tube_;
                const Config              config_;
                
                const std::string         redis_signal_channel_;
                const std::string         redis_key_prefix_;
                const std::string         redis_channel_prefix_;
                
                const Json::Value         default_validity_;
                
            protected: // Logs Data
                
                ev::Loggable::Data        loggable_data_;
                
            private: // Data
                
                int64_t                   id_;
                std::string               channel_;
                int64_t                   validity_;
                bool                      transient_;
                ProgressReport            progress_report_;
                bool                      cancelled_;

                Json::Value               progress_;
                Json::Value               errors_array_;
                
                Json::Value               follow_up_jobs_;
                Json::Value               follow_up_payload_;
                std::string               follow_up_id_key_;
                std::string               follow_up_key_;
                std::string               follow_up_tube_;
                int64_t                   follow_up_expires_in_;
                uint32_t                  follow_up_ttr_;

            private: // Data
                
                cc::UTCTime::HumanReadable chdir_hrt_;
                char                       hrt_buffer_[27];
                std::string                output_directory_prefix_;
                std::string                output_directory_;
                
            private: // Helpers
                
                ev::curl::HTTP             http_;
                ::ev::postgresql::JSONAPI  json_api_;

            protected:
                
                Json::Reader               json_reader_;
                Json::FastWriter           json_writer_;
                Json::StyledWriter         json_styled_writer_;
                
            private: // Ptrs
                
                const MessagePumpCallbacks* callbacks_ptr_;
                
            public: // Constructor(s) / Destructor
                
                Job () = delete;
                Job (const std::string& a_tube, const Config& a_config, const ev::Loggable::Data& a_loggable_data);
                virtual ~Job ();
                
            public:
                
                int64_t ID       () const;
                int64_t Validity () const;
                
            public: // Method(s) / Function(s)
                
                void Setup   (const MessagePumpCallbacks* a_callbacks, const std::string& a_output_directory_prefix);
                void Consume (const int64_t& a_id, const Json::Value& a_payload,
                              const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback);
                
            protected: // Pure Virtual Method(s) / Function(s)
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback) = 0;
                
            protected: // Method(s) / Function(s)
                
                void ConfigJSONAPI         (const Json::Value& a_config);
                
                void SetCompletedResponse  (Json::Value& o_response);
                void SetCancelledResponse  (const Json::Value& a_payload, Json::Value& o_response);
                void SetFailedResponse     (uint16_t a_code, Json::Value& o_response);                
                void SetFailedResponse     (uint16_t a_code, const Json::Value& a_payload, Json::Value& o_response);
                
            protected: // REDIS Helper Method(s) / Function(s)
                                
                void PublishProgress  (const Json::Value& a_payload); // TODO CHECK USAGE and remove it ?
                
                void AppendError       (const Json::Value& a_error);
                void AppendError       (const char* const a_type, const std::string& a_why, const char *const a_where, const int a_code);
                bool HasErrorsSet      () const;
                
                void Publish           (const Progress& a_progress);
                void Broadcast         (const Status a_status);
                void Finished          (const Json::Value& a_response,
                                        const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback);

                bool           WasCancelled      () const;
                bool           ShouldCancel      ();
                bool&          cancellation_flag ();
                
            protected: // REDIS / BEANSTALK Helper Method(s) / Function(s)
                
                bool         HasFollowUpJobs         () const;
                uint64_t     FollowUpJobsCount       () const;
                Json::Value& AppendFollowUpJob       ();
                bool         SubmitFollowUpJobs      ();
            
            private: // REDIS / BEANSTALK Helper Method(s) / Function(s)
                
                void         SubmitFollowUpJob       (const size_t a_number, const Json::Value& a_job);

            private: // REDIS Helper Method(s) / Function(s)

                void Publish (const std::string& a_channel, const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void Publish (const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
            protected: // Threading Helper Methods(s) / Function(s)
                
                void ExecuteOnMainThread (std::function<void()> a_callback, bool a_blocking);
                void OnFatalException    (const ev::Exception& a_exception);
                
            protected: // Beanstalk Helper Method(s) / Function(s)
                
                void SubmitJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr);

            protected: // JSONAPI Method(s) / Function(s)
                
                void SetJSONAPIConfig (const Json::Value& a_config);
                void JSONAPIGet       (const Json::Value& a_urn,
                                       uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_query
                );

            protected: // HTTP Method(s) / Function(s)
                
                void HTTPGet (const Json::Value& a_url,
                              uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_url);

            protected: // FILE Method(s) / Function(s)
                
                void LoadFile (const Json::Value& a_uri,
                               uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_uri);

            protected: // PostgreSQL Helper Methods(s) / Function(s)
                
                virtual void ExecuteQuery            (const std::string& a_query, Json::Value& o_result,
                                                      const bool a_use_column_name);
                virtual void ExecuteQueryWithJSONAPI (const std::string& a_query, Json::Value& o_result);
                
            protected: // Output Helper Methods(s) / Function(s)

                const std::string& EnsureOutputDir (const int64_t a_validity = -1);
                
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
             * @return Current job ID.
             */
            inline int64_t Job::ID () const
            {
                return id_;
            }            

            /**
             * @return Current job validity.
             */
            inline int64_t Job::Validity () const
            {
                return validity_;
            }

            /**
             * @return True if the job was cancelled, false otherwise.
             */
            inline bool Job::WasCancelled() const
            {
                return cancelled_;
            }
            
            /**
             * @return A reference to the cancellation flag.
             */
            inline bool& Job::cancellation_flag ()
            {
                return cancelled_;
            }
            
            inline bool Job::HasErrorsSet () const
            {
                return ( false == cancelled_ && 0 != errors_array_.size() );
            }
            
            inline bool Job::HasFollowUpJobs () const
            {
                return ( FollowUpJobsCount()> 0 );
            }
            
            inline uint64_t Job::FollowUpJobsCount () const
            {
                return ( true == follow_up_jobs_.isArray() ? static_cast<uint64_t>(follow_up_jobs_.size()) : 0 );
            }
            
            
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_JOB_H_



