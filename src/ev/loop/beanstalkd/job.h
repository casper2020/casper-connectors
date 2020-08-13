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

#include "ev/loop/beanstalkd/object.h"

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/redis/subscriptions/manager.h"

#include "ev/scheduler/scheduler.h"

#include "ev/loggable.h"

#include "ev/beanstalk/producer.h"

#include "ev/curl/http.h"
#include "ev/postgresql/json_api.h"
#include "ev/postgresql/value.h"

#include "json/json.h"

#include "ev/logger.h"
#define EV_LOOP_BEANSTALK_JOB_LOG_DEFINE_CONST(a_type, a_name, a_value) \
    a_type a_name = a_value

#define EV_LOOP_BEANSTALK_JOB_LOG_QUEUE(a_stage, a_format, ...) \
ev::LoggerV2::GetInstance().Log(logger_client_, "queue", \
    "Job #" INT64_FMT_MAX_RA ": %12.12s: " a_format, \
    ID(), a_stage, __VA_ARGS__ \
);

#define EV_LOOP_BEANSTALK_JOB_LOG(a_token, a_format, ...) \
    ev::LoggerV2::GetInstance().Log(logger_client_, a_token, \
        "Job #" INT64_FMT_MAX_RA ": " a_format, \
        ID(), __VA_ARGS__ \
    );
#define EV_LOOP_BEANSTALK_JOB_LOG_JS(a_client, a_token, a_format, ...) \
    ev::LoggerV2::GetInstance().Log(a_client, a_token, \
        "Job #" INT64_FMT_MAX_RA ": " a_format, \
        __VA_ARGS__ \
    );

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Job : protected ::ev::loop::beanstalkd::Object,
                        private ev::scheduler::Client, protected ev::redis::subscriptions::Manager::Client, public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            public: // Data Type
                
                class Config : public ::cc::NonMovable
                {
                    
                protected: // Data
                    
                    pid_t       pid_;
                    uint64_t    instance_;
                    std::string service_id_;
                    bool        transient_;
                    int         min_progress_;
                    int         log_level_;
                    Json::Value other_;
                    
                public: // Constructor(s) / Destructor
                    
                    Config () = delete;
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_pid
                     * @param a_instance
                     * @param a_service_id
                     * @param a_transient
                     * @param a_min_progress
                     * @param a_log_level
                     * @param a_other
                     */
                    Config (const pid_t& a_pid, const uint64_t& a_instance, const std::string& a_service_id, const bool a_transient,
                            const int a_min_progress = 3,
                            const int a_log_level = -1,
                            const Json::Value& a_other = Json::Value::null)
                        : pid_(a_pid), instance_(a_instance), service_id_(a_service_id), transient_(a_transient), min_progress_(a_min_progress), log_level_(a_log_level),
                          other_(a_other)
                    {
                        /* empty */
                    }
                    
                    /**
                     * @brief Copy constructor.
                     *
                     * @param a_config Object to copy.
                     */
                    Config (const Config& a_config)
                    : pid_(a_config.pid_), instance_(a_config.instance_), service_id_(a_config.service_id_), transient_(a_config.transient_),   min_progress_(a_config.min_progress_), log_level_(a_config.log_level_), other_(a_config.other_)
                    {
                        /* empty */
                    }
                    
                    /**
                     * @brief Assigment operator overload.
                     *
                     * @param a_config Object to copy.
                     *
                     * @return Ref to this object.
                     */
                    inline Config& operator=(const Config& a_config)
                    {
                        pid_          = a_config.pid_;
                        instance_     = a_config.instance_;
                        service_id_   = a_config.service_id_;
                        transient_    = a_config.transient_;
                        min_progress_ = a_config.min_progress_;
                        log_level_    = a_config.log_level_;
                        other_        = a_config.other_;
                        return *this;
                    }
                    
                    /**
                     * @return R/O Access to \link pid_ \link.
                     */
                    inline const pid_t& pid() const
                    {
                        return pid_;
                    }
                    
                    /**
                     * @return R/O Access to \link instance_ \link.
                     */
                    inline const uint64_t& instance () const
                    {
                        return instance_;
                    }
                    
                    /**
                     * @return R/O Access to \link service_id_ \link.
                     */
                    inline const std::string& service_id () const
                    {
                        return service_id_;
                    }
                    
                    /**
                     * @return R/O Access to \link transient_ \link.
                     */
                    inline const bool& transient () const
                    {
                        return transient_;
                    }
                    
                    /**
                     * @return R/O Access to \link min_progress_ \link.
                     */
                    inline const int& min_progress () const
                    {
                        return min_progress_;
                    }
                    
                    /**
                     * @return R/O Access to \link log_level_ \link.
                     */
                    inline const int& log_level () const
                    {
                        return log_level_;
                    }
                    
                    /**
                     * @return R/O Access to \link other_ \link.
                     */
                    inline const Json::Value& other ()  const
                    {
                        return other_;
                    }
                    
                    /**
                     * @brief Set PID and instance.
                     *
                     * @param a_pid Process identifier.
                     */
                    inline void SetPID (const pid_t& a_pid)
                    {
                        pid_ = a_pid;
                    }
                    
                    /**
                     * @brief Set instance.
                     *
                     * @param a_instance Instance number.
                     */
                    inline void SetInstance (const uint64_t& a_instance)
                    {
                        instance_ = a_instance;
                    }
                    
                    /**
                     * @brief Set service ID.
                     *
                     * @param Service ID value.
                     */
                    inline void SetServiceID (const std::string& a_service_id)
                    {
                        service_id_ = a_service_id;
                    }
                    
                    /**
                     * @brief Set if it's transient or not.
                     *
                     * @param a_transient Bool value.
                     */
                    inline void SetTransient (const bool& a_transient)
                    {
                        transient_ = a_transient;
                    }
                    
                }; // end of class 'Config';
                
                typedef std::function<void(const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code)> CompletedCallback;
                typedef std::function<void(bool a_already_ran)>                                                                CancelledCallback;
                typedef std::function<void()>                                                                                  DeferredCallback;

                typedef std::function<void(const ev::Exception&)>                                                                                                         FatalExceptionCallback;
                typedef std::function<void(std::function<void()> a_callback, bool a_blocking)>                                                                            DispatchOnThread;
                typedef std::function<void(const std::string& a_id, std::function<void(const std::string&)> a_callback, const size_t a_deferred, const bool a_recurrent)> DispatchOnThreadDeferred;
                typedef std::function<void(const std::string& a_id)>                                                                                                      CancelDispatchOnThread;
                typedef std::function<void(const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)>                                               PushJobCallback;
                
                typedef struct {
                    FatalExceptionCallback   on_fatal_exception_;
                    DispatchOnThread         on_main_thread_;
                    DispatchOnThreadDeferred schedule_callback_on_the_looper_thread_;
                    CancelDispatchOnThread   try_cancel_callback_on_the_looper_thread_;
                    PushJobCallback          on_push_job_;
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
                
                typedef std::function<void(const std::string&)> LooperThreadCallback;
                
            protected: // Const Static Data
                
                constexpr static const int k_exception_rc_ = -3;

            protected: // Const Data
                
                const std::string         tube_;
                const Config              config_;
                
                const std::string         redis_signal_channel_;
                const std::string         redis_key_prefix_;
                const std::string         redis_channel_prefix_;
                
                const Json::Value         default_validity_;
                
            private: // Data
                
                int64_t                   id_;
                std::string               channel_;
                int64_t                   validity_;
                bool                      transient_;
                ProgressReport            progress_report_;
                bool                      cancelled_;
                bool                      already_ran_;
                bool                      deferred_;

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
                std::string                logs_directory_;
                
            private: // Helpers
                
                ev::curl::HTTP             http_;
                ::ev::postgresql::JSONAPI  json_api_;
                
            protected:
                
                Json::Reader               json_reader_;
                Json::FastWriter           json_writer_;
                Json::StyledWriter         json_styled_writer_;
                
            private: // Ptrs
                
                const MessagePumpCallbacks* callbacks_ptr_;

            private: // Stats
                
                std::chrono::steady_clock::time_point start_tp_;
                std::chrono::steady_clock::time_point end_tp_;
                
            protected: // callbacks
                
                std::function<void(const char* const a_tube, const char* const a_key, const std::string& a_value)> owner_log_callback_;

            public: // Constructor(s) / Destructor
                
                Job () = delete;
                Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config);
                virtual ~Job ();
                
            public:
                
                int64_t ID       () const;
                int64_t Validity () const;
                
            public: // Method(s) / Function(s)
                
                void Setup   (const MessagePumpCallbacks* a_callbacks, const std::string& a_output_directory_prefix, const std::string& a_logs_directory);
                void Consume (const int64_t& a_id, const Json::Value& a_payload,
                              const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback, const DeferredCallback& a_deferred_callback);

            public: // Other API Virtual Method(s) / Function(s)
                    
                void SetOwnerLogCallback (std::function<void(const char* const a_tube, const char* const a_key, const std::string& a_value)> a_callback);

            protected: // Optional Virtual Method(s) / Function(s)
                
                virtual void Setup () { };

            protected: // Pure Virtual Method(s) / Function(s)

                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback, const DeferredCallback& a_deferred_callback) = 0;
                
            protected: // Method(s) / Function(s)
                
                void ConfigJSONAPI         (const Json::Value& a_config);
                
                void SetCompletedResponse  (Json::Value& o_response);
                void SetCompletedResponse  (const Json::Value& a_payload, Json::Value& o_response);
                void SetCancelledResponse  (const Json::Value& a_payload, Json::Value& o_response);
                void SetFailedResponse     (uint16_t a_code, Json::Value& o_response);
                void SetFailedResponse     (uint16_t a_code, const Json::Value& a_payload, Json::Value& o_response);

                void SetTimeoutResponse    (const Json::Value& a_payload, Json::Value& o_response);

            protected: // REDIS Helper Method(s) / Function(s)
                                
                void PublishProgress  (const Json::Value& a_payload); // TODO CHECK USAGE and remove it ?
                
                void   AppendError       (const Json::Value& a_error);
                void   AppendError       (const char* const a_type, const std::string& a_why, const char *const a_where, const int a_code);
                bool   HasErrorsSet      () const;
                size_t ErrorsCount        () const;
                const Json::Value& LastError () const;
                
                void Publish           (const Progress& a_progress);
                void Publish           (const std::vector<ev::loop::beanstalkd::Job::Progress>& a_progress);
                void Broadcast         (const Status a_status);
                void Finished          (const Json::Value& a_response,
                                        const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback);

                void Relay             (const uint64_t& a_id, const std::string& a_fq_channel, const Json::Value& a_object);
                void Finished          (const uint64_t& a_id, const std::string& a_fq_channel, const Json::Value& a_response,
                                        const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback);
                void Broadcast         (const uint64_t& a_id, const std::string& a_fq_channel, const Status a_status);

                bool  WasCancelled      () const;
                bool  AlreadyRan        () const;
                void  SetDeferred       ();
                bool  Deferred          () const;
                bool  ShouldCancel      ();
                bool& cancellation_flag ();
                                
            protected: // REDIS / BEANSTALK Helper Method(s) / Function(s)
                
                bool         HasFollowUpJobs   () const;
                uint64_t     FollowUpJobsCount () const;
                Json::Value& AppendFollowUpJob ();
                bool         PushFollowUpJobs  ();
                
            protected: // Other Settings
                
                const std::string& logs_directory () const;
                
            protected: // Stats
                
                const std::chrono::steady_clock::time_point& start_tp () const;
            
            private: // REDIS / BEANSTALK Helper Method(s) / Function(s)
                
                void         SubmitFollowUpJob       (const size_t a_number, const Json::Value& a_job);

            private: // REDIS Helper Method(s) / Function(s)

                void Publish (const uint64_t& a_id, const std::string& a_fq_channel, const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void Publish (const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
            protected: // Threading Helper Methods(s) / Function(s)
                
                void ExecuteOnMainThread    (std::function<void()> a_callback, bool a_blocking);
                
            protected: // Threading Helper Methods(s) / Function(s)
                                
                void ScheduleCallbackOnLooperThread (const std::string& a_id, LooperThreadCallback a_callback,
                                                     const size_t a_deferred = 0, const bool a_recurrent = false);
                
                void TryCancelCallbackOnLooperThread (const std::string& a_id);

            protected: // Other Methods(s) / Function(s)

                void OnFatalException       (const ev::Exception& a_exception);

                
            protected: // Beanstalk Helper Method(s) / Function(s)
                
                void PushJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr);

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
                
                const char* const JSONValueTypeAsCString (const Json::ValueType& a_type) const;
                
                const Json::Value& GetJSONObject (const Json::Value& a_parent, const char* const a_key, const Json::ValueType& a_type, const Json::Value* a_default,
                                                  const char* const a_error_prefix_msg = "Invalid or missing ") const;
                
                void        ParseJSON      (const std::string& a_value, Json::Value& o_value)           const;
                void        ToJSON         (const ev::postgresql::Value& a_value, Json::Value& o_value) const; 
                
            protected: //
                
                ev::scheduler::Task*                             NewTask                (const EV_TASK_PARAMS& a_callback);
                EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK JobSignalsDataCallback (const std::string& a_name, const std::string& a_message);
                
            private: // from ::ev::redis::Subscriptions::Manager::Client
                
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
            inline bool Job::WasCancelled () const
            {
                return cancelled_;
            }
        
            /**
             * @return True if the job was cancelled, false otherwise.
             */
            inline bool Job::AlreadyRan () const
            {
                return already_ran_;
            }
        
            /**
             * @return Set when response will be deferred.
             */
            inline void Job::SetDeferred ()
            {
                deferred_ = true;
            }
    
            /**
             * @return True if the job was deferred, false otherwise.
             */
            inline bool Job::Deferred () const
            {
                return deferred_;
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
        
            inline size_t Job::ErrorsCount () const
            {
                return static_cast<size_t>(errors_array_.size());
            }
        
            inline const Json::Value& Job::LastError () const
            {
                return ( errors_array_.size() > 0 ? errors_array_[errors_array_.size() - 1] : Json::Value::null );
            }
            
            inline bool Job::HasFollowUpJobs () const
            {
                return ( FollowUpJobsCount()> 0 );
            }
            
            inline uint64_t Job::FollowUpJobsCount () const
            {
                return ( true == follow_up_jobs_.isArray() ? static_cast<uint64_t>(follow_up_jobs_.size()) : 0 );
            }
            
            inline const std::chrono::steady_clock::time_point& Job::start_tp () const
            {
                return start_tp_;
            }
        
            /*
             * @return R/O access to logs directory.
             */
            inline const std::string& Job::logs_directory () const
            {
                return logs_directory_;
            }
        
            inline const char* const Job::JSONValueTypeAsCString (const Json::ValueType& a_type) const
            {
                
                switch (a_type) {
                    case Json::ValueType::nullValue:
                        return "null";
                    case Json::ValueType::intValue:
                        return "int";
                    case Json::ValueType::uintValue:
                        return "uint";
                    case Json::ValueType::realValue:
                        return "real";
                    case Json::ValueType::stringValue:
                        return "string";
                    case Json::ValueType::booleanValue:
                        return "boolean";
                    case Json::ValueType::arrayValue:
                        return "array";
                    case Json::ValueType::objectValue:
                        return "object";
                    default:
                        return "???";
                }
            
            }
        
            inline void Job::SetOwnerLogCallback (std::function<void(const char* const a_tube, const char* const a_key, const std::string& a_value)> a_callback)
            {
                owner_log_callback_ = a_callback;
            }
            
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_JOB_H_



