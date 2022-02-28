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
#include "ev/loop/beanstalkd/config.h"

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

#include "cc/bitwise_enum.h"

#include "cc/macros.h"

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
                
                
                typedef std::function<Job*(const std::string& a_tube)> Factory;

                
                class Config : public ::cc::NonMovable
                {
                    
                protected: // Data
                    
                    pid_t              pid_;
                    uint64_t           instance_;
                    int                cluster_;
                    std::string        service_id_;
                    bool               transient_;
                    int                min_progress_;
                    int                log_level_;
                    bool               log_redact_;
                    std::string        log_token_;
                    Json::Value        other_;
                    std::set<uint16_t> dnbe_;
                    
                public: // Constructor(s) / Destructor
                    
                    Config () = delete;
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_pid
                     * @param a_instance
                     * @param a_cluster
                     * @param a_service_id
                     * @param a_transient
                     * @param a_min_progress
                     * @param a_log_level
                     * @param a_log_redact
                     * @param a_log_token
                     * @param a_other
                     * @param a_dnbe
                     */
                    Config (const pid_t& a_pid, const uint64_t& a_instance, const int& a_cluster, const std::string& a_service_id, const bool a_transient,
                            const int a_min_progress,
                            const int a_log_level, const bool a_log_redact, const std::string& a_log_token,
                            const Json::Value& a_other, const std::set<uint16_t>& a_dnbe)
                        : pid_(a_pid), instance_(a_instance), cluster_(a_cluster), service_id_(a_service_id), transient_(a_transient), min_progress_(a_min_progress),
                          log_level_(a_log_level), log_redact_(a_log_redact), log_token_(a_log_token), other_(a_other), dnbe_(a_dnbe)
                    {
                        /* empty */
                    }
                    
                    /**
                     * @brief Copy constructor.
                     *
                     * @param a_config Object to copy.
                     */
                    Config (const Config& a_config)
                    : pid_(a_config.pid_), instance_(a_config.instance_), cluster_(a_config.cluster_), service_id_(a_config.service_id_), transient_(a_config.transient_),   min_progress_(a_config.min_progress_),
                        log_level_(a_config.log_level_), log_redact_(a_config.log_redact_), log_token_(a_config.log_token_), other_(a_config.other_), dnbe_(a_config.dnbe_)
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
                        cluster_      = a_config.cluster_;
                        service_id_   = a_config.service_id_;
                        transient_    = a_config.transient_;
                        min_progress_ = a_config.min_progress_;
                        log_level_    = a_config.log_level_;
                        log_redact_   = a_config.log_redact_;
                        log_token_    = a_config.log_token_;
                        other_        = a_config.other_;
                        dnbe_         = a_config.dnbe_;
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
                     * @return R/O Access to \link cluster_ \link.
                     */
                    inline const int& cluster () const
                    {
                        return cluster_;
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
                     * @return R/O Access to \link log_token_ \link.
                     */
                    inline const std::string& log_token () const
                    {
                        return log_token_;
                    }

                    /**
                     * @return R/O Access to \link log_redact_ \link.
                     */
                    inline const bool& log_redact () const
                    {
                        return log_redact_;
                    }
                    
                    /**
                     * @return R/O Access to \link other_ \link.
                     */
                    inline const Json::Value& other () const
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
                     * @brief Set cluster.
                     *
                     * @param a_cluster Cluster number.
                     */
                    inline void SetCluster (const int& a_cluster)
                    {
                        cluster_ = a_cluster;
                    }
                    
                    /**
                     * @brief Set service ID.
                     *
                     * @param a_service_id ID value.
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
                                        
                    /**
                     * @brief Set min progress value.
                     *
                     * @param a_value Int value.
                     */
                    inline void SetMinProgress (const int& a_value)
                    {
                        min_progress_ = a_value;
                    }
                    
                    /**
                     * @brief Check if this job should be buried or not.
                     *
                     * @param a_code HTTP status code.
                     *
                     * @return True if job should be buried, false otherwise.
                     */
                    inline const bool Bury (const uint16_t a_code) const
                    {
                        return ( dnbe_.end() == dnbe_.find(a_code) );
                    }
                    
                }; // end of class 'Config';
                
                typedef std::function<void(const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code)> CompletedCallback;
                typedef std::function<void(bool a_already_ran)>                                                                CancelledCallback;
                
                typedef std::function<void(const int64_t&, const std::string&)>                                                DeferredCallback;
                typedef std::function<void(const int64_t&, const std::string&)>                                                FinishedCallback;

                typedef std::function<void(const ev::Exception&)>                                                                                                         FatalExceptionCallback;
                typedef std::function<void(std::function<void()> a_callback, bool a_blocking)>                                                                            DispatchOnMainThread;
                typedef std::function<void(std::function<void()> a_callback, const size_t a_deferred)>                                                                    DispatchOnMainThreadDeferred;
                typedef std::function<void(const std::string& a_id, std::function<void(const std::string&)> a_callback, const size_t a_deferred, const bool a_recurrent)> DispatchOnThreadDeferred;
                typedef std::function<void(const std::string& a_id)>                                                                                                      CancelDispatchOnThread;
                typedef std::function<void(const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)>                                               PushJobCallback;
                                
                typedef struct {
                    FatalExceptionCallback       on_fatal_exception_;
                    DispatchOnMainThread         dispatch_on_main_thread_;
                    DispatchOnMainThreadDeferred schedule_on_main_thread_;
                    DispatchOnThreadDeferred     schedule_callback_on_the_looper_thread_;
                    CancelDispatchOnThread       try_cancel_callback_on_the_looper_thread_;
                    PushJobCallback              on_push_job_;
                } MessagePumpCallbacks;
                                
            protected: // Data Type(s)
                
                enum class Status : uint8_t {
                    InProgress = 0,
                    Finished,
                    Failed,
                    Cancelled,
                };
                
                enum class Mode : uint8_t {
                    Default,
                    Gateway
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
                
                enum class ResponseFlags : uint8_t {
                    None        = 0,
                    SuccessFlag = 1 << 0,
                    All         = 0xFF
                };
                
            protected: // Const Static Data
                
                constexpr static const int k_exception_rc_ = -3;

            protected: // Const Data
                
                const std::string         tube_;
                const Config              config_;
                
                const std::string         redis_signal_channel_;
                const std::string         redis_key_prefix_;
                const std::string         redis_channel_prefix_;
                
                const Json::Value         default_ttr_;
                const Json::Value         default_validity_;
                
            private: // Data
                
                int64_t                   bjid_;                //!< BEANSTALKD job id.
                std::string               rjnr_;                //!< REDIS job id.
                std::string               rjid_;                //!< REDIS job key.
                std::string               rcid_;                //!< REDIS job channel.
                int64_t                   ttr_;
                int64_t                   validity_;
                int64_t                   expires_in_;
                bool                      transient_;
                ProgressReport            progress_report_;
                bool                      cancelled_;
                bool                      already_ran_;
                bool                      deferred_;

                Json::Value               progress_;            //!< This object must be always an objectValue!
                
            private: // Data
                
                cc::UTCTime::HumanReadable chdir_hrt_;
                char                       hrt_buffer_[27];
                std::string                output_directory_prefix_;
                std::string                output_directory_;
                std::string                logs_directory_;
                std::string                shared_directory_;
                ResponseFlags              response_flags_;

            private: // Helpers
                
                ev::curl::HTTP             http_;
                ::ev::postgresql::JSONAPI  json_api_;

            private: // Ptrs
                
                const MessagePumpCallbacks* callbacks_ptr_;

            private: // Stats
                
                std::chrono::steady_clock::time_point start_tp_;
                std::chrono::steady_clock::time_point end_tp_;
                
            protected: // callbacks
                
                typedef std::function<void(const char* const, const char* const, const std::string&)> OwnerLogCallback;
                typedef std::function<void(const uint64_t&, const std::string&, const Json::Value&)>  SignalsChannelListerer;
                
                SignalsChannelListerer signals_channel_listener_;
                OwnerLogCallback       owner_log_callback_;
                FinishedCallback       finished_callback_;
                
            public: // Constructor(s) / Destructor
                
                Job () = delete;
                Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config,
                     const ResponseFlags a_response_flags = ResponseFlags::All);
                virtual ~Job ();
                
            public:
                
                void               SetTTRAndValidity (const uint64_t& a_ttr, const uint64_t& a_validity);
                const int64_t&     TTR               () const;
                const int64_t&     Validity          () const;
                const int64_t&     ExpiresIn         () const;

                const int64_t&     ID      () const;
                const std::string& RJNR    () const;
                const std::string& RJID    () const;
                const std::string& RCID    () const;
                const bool         Bury    (const uint16_t a_code) const;
                
            public: // Method(s) / Function(s)
                
                void Setup   (const MessagePumpCallbacks* a_callbacks,
                              const ::ev::loop::beanstalkd::SharedConfig& a_shared_config, FinishedCallback a_finished);
                void Dismantle (const ::cc::Exception* a_cc_exception);
                void Consume (const int64_t& a_id, const Json::Value& a_payload,
                              const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback, const DeferredCallback& a_deferred_callback);

            public: // Method(s) / Function(s)
                    
                void SetOwnerLogCallback       (OwnerLogCallback a_callback);
                void SetSignalsChannelListerer (SignalsChannelListerer a_listener);
                void SetOutputDirectoryPrefix  (const std::string& a_prefix);

            protected: // Optional Virtual Method(s) / Function(s)
                
                virtual void Setup     () { }
                virtual void Dismantle () { }

            protected: // Pure Virtual Method(s) / Function(s)

                virtual void Run (const int64_t& a_id, const Json::Value& a_payload,
                                  const CompletedCallback& a_completed_callback, const CancelledCallback& a_cancelled_callback, const DeferredCallback& a_deferred_callback) = 0;
                
            protected: // Method(s) / Function(s)
                
                void ConfigJSONAPI (const Json::Value& a_config);
                
            private: // Method(s) / Function(s)
                
                uint16_t FillResponseObject (const uint16_t& a_code, const char* const a_status, const Json::Value& a_response,
                                             Json::Value& o_object,
                                             const char* const a_action = "response")  const;
            protected: // Method(s) / Function(s)
                
                // 2xx
                uint16_t SetCompletedResponse            (Json::Value& o_response) const;
                uint16_t SetCompletedResponse            (const Json::Value& a_payload, Json::Value& o_response) const;
                void     SetCancelledResponse            (const Json::Value& a_payload, Json::Value& o_response) const;

                // 3xx
                uint16_t SetRedirectResponse             (const Json::Value& a_payload, Json::Value& o_response, const uint16_t a_code = 302) const;

                // 4xx
                uint16_t SetBadRequestResponse           (const std::string& a_why, Json::Value& o_response) const;
                uint16_t SetTimeoutResponse              (const Json::Value& a_payload, Json::Value& o_response) const;
                
                // 5xx
                uint16_t SetInternalServerErrorResponse  (const std::string& a_why, Json::Value& o_response) const;
                uint16_t SetNotImplementedResponse       (const Json::Value& a_payload, Json::Value& o_response) const;

                // 4xx, 50xx
                uint16_t SetFailedResponse               (const uint16_t& a_code, Json::Value& o_response) const;
                uint16_t SetFailedResponse               (const uint16_t& a_code, const Json::Value& a_payload, Json::Value& o_response) const;

                //
                void  SetProgress                        (const ev::loop::beanstalkd::Job::Progress& a_progress, Json::Value& o_progress) const;

            protected: // REDIS Helper Method(s) / Function(s)
                                
                void PublishProgress  (const Json::Value& a_payload); // TODO CHECK USAGE and remove it ?

                void Publish           (const Progress& a_progress);
                void Publish           (const std::vector<ev::loop::beanstalkd::Job::Progress>& a_progress);
                void Finished          (const Json::Value& a_response,
                                        const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback);

                void Relay             (const uint64_t& a_id, const std::string& a_fq_channel, const std::string& a_fq_key, const Json::Value& a_object);
                void Finished          (const uint64_t& a_id, const std::string& a_fq_channel, const std::string& a_fq_key, const Json::Value& a_response,
                                        const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback,
                                        const Mode a_mode = Mode::Default);
                void Publish           (const uint64_t& a_id, const std::string& a_fq_channel, const std::string& a_fq_key, const Progress& a_progress);
                void Broadcast         (const uint64_t& a_id, const std::string& a_fq_channel, const Status a_status);
                
                void Cancel            (const uint64_t& a_id, const std::string& a_fq_channel, const std::string& a_fq_key,
                                        const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
                void SetStatus         (const uint64_t& a_id, const std::string& a_fq_key,
                                        const std::string& a_status, const int64_t* a_expires_in = nullptr,
                                        const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);

                bool  WasCancelled      () const;
                bool  AlreadyRan        () const;
                void  SetDeferred       ();
                bool  Deferred          () const;
                bool  ShouldCancel      ();
                bool& cancellation_flag ();

            protected: // Other Settings
                
                const std::string& logs_directory    () const;
                const std::string& shared_directory  () const;
                const std::string& output_dir_prefix () const;
            
            protected: // Response Options
                
                void Reset (const ResponseFlags& a_flag);
                void Set   (const ResponseFlags& a_flag);
                void Unset (const ResponseFlags& a_flag);

            private: // REDIS Helper Method(s) / Function(s)

                void Publish (const uint64_t& a_id, const std::string& a_fq_channel, const std::string& a_fq_key,
                              const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr,
                              const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr,
                              const bool a_final = false, const Mode a_mode = Mode::Default);

                void Publish (const uint64_t& a_id, const std::string& a_fq_channel, const std::string& a_fq_key,
                              const std::string& a_message, const std::string& a_status,
                              const std::function<void()> a_success_callback = nullptr,
                              const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr,
                              const bool a_final = false);
                
                void Publish (const Json::Value& a_object,
                              const std::function<void()> a_success_callback = nullptr, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback = nullptr);
                
            protected: // Threading Helper Methods(s) / Function(s)
                
                void ExecuteOnMainThread  (std::function<void()> a_callback, bool a_blocking);
                void ScheduleOnMainThread (std::function<void()> a_callback, const size_t a_deferred);
                
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
                              uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_url,
                              const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);
                
                void HTTPGet (const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers,
                              EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                              const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

                void HTTPGetFile (const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers,
                                  const uint64_t& a_validity, const std::string& a_prefix, const std::string& a_extension,
                                  EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                                  const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

                void HTTPPost (const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body,
                               EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                               const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

                void HTTPPostFile (const std::string& a_uri, const std::string& a_url,
                                   const EV_CURL_HEADERS_MAP& a_headers,
                                   EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                                   const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

            protected: // FILE Method(s) / Function(s)
                
                void LoadFile (const Json::Value& a_uri,
                               uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_uri);

            protected: // PostgreSQL Helper Methods(s) / Function(s)
                
                virtual void ExecuteQuery            (const std::string& a_query, Json::Value& o_result,
                                                      const bool a_dont_auto_parse);
                virtual void ExecuteQueryWithJSONAPI (const std::string& a_query, Json::Value& o_result);
                
            protected: // Output Helper Methods(s) / Function(s)

                const std::string& EnsureOutputDir (const int64_t a_validity = -1);
                
            protected: // JsonCPP Helper Methods(s) / Function(s)
                
                const char* const JSONValueTypeAsCString (const Json::ValueType& a_type) const;
                
                const Json::Value& GetJSONObject (const Json::Value& a_parent, const char* const a_key, const Json::ValueType& a_type, const Json::Value* a_default,
                                                  const char* const a_error_prefix_msg = "Invalid or missing ") const;
                
                void MergeJSONValue (Json::Value& a_lhs, const Json::Value& a_rhs) const;
                
                void        ParseJSON      (const std::string& a_value, Json::Value& o_value)           const;
                void        ToJSON         (const ev::postgresql::Value& a_value, Json::Value& o_value) const;
                void        ToJSON         (const ev::curl::Value& a_value, Json::Value& o_value)       const;
                
            protected: //
                
                ev::scheduler::Task*                             NewTask                (const EV_TASK_PARAMS& a_callback);

            private: // ::ev::redis::Subscriptions Callbacks

                EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK OnSignalsChannelMessageReceived (const std::string& a_id, const std::string& a_message);
                                
            private: // from ::ev::redis::Subscriptions::Manager::Client
                
                virtual void OnREDISConnectionLost ();
                
            private: // Methods(s) / Function(s)
                
                void HTTPSyncExec (std::function<void(EV_CURL_HTTP_SUCCESS_CALLBACK, EV_CURL_HTTP_FAILURE_CALLBACK)> a_block,
                                   EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback);
                
            protected: // Inline Methods(s) / Function(s) - Stats
                
                const std::chrono::steady_clock::time_point& start_tp () const;

            }; // end of class 'Job'
            
            /**
             * @brief Override TTR and validity values.
             *
             * @param a_ttr      TTR in seconds.
             * @param a_validity Validity in seconds.
             */
            inline void Job::SetTTRAndValidity (const uint64_t& a_ttr, const uint64_t& a_validity)
            {
                ttr_        = a_ttr;
                validity_   = a_validity;
                expires_in_ = ttr_ + validity_;
            }
            
            /**
             * @return Current job ttr.
             */
            inline const int64_t& Job::TTR () const
            {
                return ttr_;
            }

            /**
             * @return Current job validity.
             */
            inline const int64_t& Job::Validity () const
            {
                return validity_;
            }
            
            /**
             * @return Current job expires in.
             */
            inline const int64_t& Job::ExpiresIn () const
            {
                return expires_in_;
            }

            /**
             * @return Current job ID.
             */
            inline const int64_t& Job::ID () const
            {
                return bjid_;
            }

            /**
             * @return REDIS job number.
             */
            inline const std::string& Job::RJNR () const
            {
                return rjnr_;
            }
        
            /**
             * @return REDIS job key.
             */
            inline const std::string& Job::RJID () const
            {
                return rjid_;
            }

            /**
             * @return REDIS job channel.
             */
            inline const std::string& Job::RCID () const
            {
                return rcid_;
            }
        
            /**
             * @param Check if this job should be buried or not.
             *
             * @param a_code HTTP status code.
             *
             * @return True if job should be buried, false otherwise.
             */
            inline const bool Job::Bury (const uint16_t a_code) const
            {
                return config_.Bury(a_code);
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

            /*
             * @return R/O access to logs directory.
             */
            inline const std::string& Job::logs_directory () const
            {
                return logs_directory_;
            }
                   
            /*
             * @return R/O access to shared directory.
             */
            inline const std::string& Job::shared_directory () const
            {
                return shared_directory_;
            }
        
            /**
             * @return R/O access to outpur directory prefix.
             */
            inline const std::string& Job::output_dir_prefix () const
            {
                return output_directory_prefix_;
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

            inline void Job::SetSignalsChannelListerer (Job::SignalsChannelListerer a_listener)
            {
                signals_channel_listener_ = a_listener;
            }

            inline void Job::SetOwnerLogCallback (Job::OwnerLogCallback a_callback)
            {
                owner_log_callback_ = a_callback;
            }
        
            inline void Job::SetOutputDirectoryPrefix (const std::string& a_prefix)
            {
                output_directory_        = "";
                output_directory_prefix_ = a_prefix;
            }
        
            DEFINE_ENUM_WITH_BITWISE_OPERATORS(Job::ResponseFlags)

            /**
             * @brief Reset the response flag value.
             *
             * @param a_flag Initial value of \link Job::ResponseFlags \link.
             */
            inline void Job::Reset (const Job::ResponseFlags& a_flag)
            {
                response_flags_ = a_flag;
            }

            /**
             * @brief Set a response option flag.
             *
             * @param a_flag \link Job::ResponseFlags \link.
             */
            inline void Job::Set (const Job::ResponseFlags& a_flag)
            {
                response_flags_ |= a_flag;
            }

            /**
             * @brief Clear a response option flag.
             *
             * @param a_flag \link Job::ResponseFlags \link.
             */
            inline void Job::Unset (const Job::ResponseFlags& a_flag)
            {
                response_flags_ &= ~(a_flag);
            }
        
            /**
             * @return R/O access to this job start timepoint.
             */
            inline const std::chrono::steady_clock::time_point& Job::start_tp () const
            {
                return start_tp_;
            }
        
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_JOB_H_



