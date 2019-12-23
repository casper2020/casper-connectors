/**
 * @file job.cc
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


#include "ev/loop/beanstalkd/job.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/reply.h"
#include "ev/postgresql/error.h"

#include "osal/condition_variable.h"

#include <fstream> // std::ifstream

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_tube
 * @param a_config
 */
ev::loop::beanstalkd::Job::Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config)
    : ::ev::loop::beanstalkd::Object(a_loggable_data),
    tube_(a_tube), config_(a_config),
    redis_signal_channel_(config_.service_id_ + ":job-signal"),
    redis_key_prefix_(config_.service_id_ + ":jobs:" + tube_ + ':'),
    redis_channel_prefix_(config_.service_id_ + ':' + tube_ + ':'),
    default_validity_(3600),
    json_api_(loggable_data_, /* a_enable_task_cancellation */ false)
{
    id_                              = 0;
    validity_                        = -1;
    transient_                       = config_.transient_;
    progress_report_.timeout_in_sec_ = config_.min_progress_;
    progress_report_.last_tp_        = std::chrono::steady_clock::time_point::max();
    cancelled_                       = false;

    progress_                        = Json::Value::null;
    errors_array_                    = Json::Value::null;
    follow_up_jobs_                  = Json::Value::null;

    chdir_hrt_.seconds_              = static_cast<uint8_t>(0);
    chdir_hrt_.minutes_              = static_cast<uint8_t>(0);
    chdir_hrt_.hours_                = static_cast<uint8_t>(0);
    chdir_hrt_.day_                  = static_cast<uint8_t>(0);
    chdir_hrt_.month_                = static_cast<uint8_t>(0);
    chdir_hrt_.year_                 = static_cast<uint16_t>(0);
    
    hrt_buffer_[0]                   = '\0';
    
    ev::scheduler::Scheduler::GetInstance().Register(this);
   
    json_writer_.omitEndingLineFeed();
    
    callbacks_ptr_ = nullptr;
    start_tp_      = std::chrono::steady_clock::time_point::min();
    end_tp_        = std::chrono::steady_clock::time_point::min();
    
    EV_LOOP_BEANSTALK_IF_LOG_ENABLED({
        ev::LoggerV2::GetInstance().Register(logger_client_, { "queue", "stats" });
    });
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Job::~Job ()
{
    ExecuteOnMainThread([this] {
        ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);
    }, /* a_blocking */ true);
    ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief One-shot setup.
 *
 * @param a_callbacks
 * @param a_output_directory_prefix
 */
void ev::loop::beanstalkd::Job::Setup (const Job::MessagePumpCallbacks* a_callbacks, const std::string& a_output_directory_prefix)
{
    osal::ConditionVariable cv;
    
    callbacks_ptr_           = a_callbacks;
    output_directory_prefix_ = a_output_directory_prefix;
    output_directory_        = a_output_directory_prefix;

    ExecuteOnMainThread([this, &cv] {
        ::ev::redis::subscriptions::Manager::GetInstance().SubscribeChannels({ redis_signal_channel_ },
                                                                             /* a_status_callback */
                                                                             [this, &cv](const std::string& a_name_or_pattern,
                                                                                         const ::ev::redis::subscriptions::Manager::Status& a_status) -> EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK {
                                                                                 if ( ::ev::redis::subscriptions::Manager::Status::Subscribed == a_status ) {
                                                                                     cv.Wake();
                                                                                 }
                                                                                 return nullptr;
                                                                             },
                                                                             /* a_data_callback */
                                                                             std::bind(&ev::loop::beanstalkd::Job::JobSignalsDataCallback, this, std::placeholders::_1, std::placeholders::_2),
                                                                             /* a_client */
                                                                             this
        );
    }, /* a_blocking */ false);
    cv.Wait();
}

/**
 * @brief Prepare a job to run.
 *
 * @param a_id
 * @param a_payload
 * @param a_completed_callback
 * @param a_cancelled_callback
 */
void ev::loop::beanstalkd::Job::Consume (const int64_t& a_id, const Json::Value& a_payload,
                                         const ev::loop::beanstalkd::Job::CompletedCallback& a_completed_callback, const ev::loop::beanstalkd::Job::CancelledCallback& a_cancelled_callback)
{
    //
    // JOB Configuration - ID is mandatory
    //
    const Json::Value redis_channel = a_payload.get("id", Json::nullValue); // id: a.k.a. redis <beanstalkd_tube_name>:<redis_job_id>
    if ( true == redis_channel.isNull() || false == redis_channel.isString() ||  0 == redis_channel.asString().length() ) {
        throw ev::Exception("Missing or invalid 'id' object!");
    }

    //
    // RESET 'job' variables
    //
    id_                              = a_id; // beanstalkd number id
    channel_                         = redis_channel.asString();
    validity_                        = a_payload.get("validity" , default_validity_).asInt64();
    transient_                       = a_payload.get("transient", config_.transient_).asBool();
    progress_report_.timeout_in_sec_ = a_payload.get("min_progress", config_.min_progress_).asInt();
    cancelled_                       = false;
    progress_                        = Json::Value(Json::ValueType::objectValue);
    errors_array_                    = Json::Value(Json::ValueType::arrayValue);
    follow_up_jobs_                  = Json::Value::null;
    follow_up_payload_               = Json::Value::null;
    follow_up_id_key_                = "";
    follow_up_key_                   = "";
    follow_up_tube_                  = "";
    follow_up_expires_in_            = -1;
    follow_up_ttr_                   = 0;
    
    //
    // JSONAPI Configuration - optional
    //
    if ( true == a_payload.isMember("jsonapi") ) {
        ConfigJSONAPI(a_payload["jsonapi"]);
    }

    //
    // Configure Log
    //
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), redis_key_prefix_ + channel_);
    
    //
    // Check for job cancellation
    //
    const auto cancelled_callback = [this, &a_cancelled_callback] () {
      
        // ... publish 'job cancelled' 'progress' ...
        Publish({
            /* key_    */ nullptr,
            /* args_   */ {},
            /* status_ */ ev::loop::beanstalkd::Job::Status::Cancelled,
            /* value_  */ -1.0
        });
        
        a_cancelled_callback();
        
    };
    
    start_tp_ = std::chrono::steady_clock::now();
    end_tp_   = start_tp_;

    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("TUBE"   , "%s", tube_.c_str());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ID"     , "%s", a_payload.get("id"  , "???").asCString());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("CHANNEL", "%s", ( redis_channel_prefix_ + channel_ ).c_str());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("KEY"    , "%s", ( redis_key_prefix_     + channel_ ).c_str());

    // ... first check if job as cancelled ...
    if ( true == ShouldCancel() ) {
        // ... notify ...
        cancelled_callback();
    } else {
        // ... execute job ...
        Run(a_id, a_payload, a_completed_callback, cancelled_callback);
    }

    end_tp_ = std::chrono::steady_clock::now();
    
    EV_LOOP_BEANSTALK_JOB_LOG_DEFINE_CONST(auto, elapsed, std::chrono::duration_cast<std::chrono::milliseconds>(end_tp_ - start_tp()).count());
    
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("DONE", "" UINT64_FMT "ms", static_cast<uint64_t>(elapsed));

    // ... prevent from REDIS cancellation messages to be logged ...
    channel_  = "";
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 *
 * @brief Configure JSONAPI.
 *
 * @param a_config
 */
void ev::loop::beanstalkd::Job::ConfigJSONAPI (const Json::Value& a_config)
{
    const Json::Value jsonapi          = GetJSONObject(a_config, "jsonapi", Json::ValueType::objectValue , &Json::Value::null);
    const Json::Value empty_string     = "";
    const Json::Value prefix           = GetJSONObject(jsonapi, "prefix"          , Json::ValueType::stringValue, &empty_string);
    const Json::Value urn              = GetJSONObject(jsonapi, "urn"             , Json::ValueType::stringValue, &empty_string);
    const Json::Value user_id          = GetJSONObject(jsonapi, "user_id"         , Json::ValueType::stringValue, &empty_string);
    const Json::Value entity_id        = GetJSONObject(jsonapi, "entity_id"       , Json::ValueType::stringValue, &empty_string);
    const Json::Value entity_schema    = GetJSONObject(jsonapi, "entity_schema"   , Json::ValueType::stringValue, &empty_string);
    const Json::Value sharded_schema   = GetJSONObject(jsonapi, "sharded_schema"  , Json::ValueType::stringValue, &empty_string);
    const Json::Value subentity_schema = GetJSONObject(jsonapi, "subentity_schema", Json::ValueType::stringValue, &empty_string);
    const Json::Value subentity_prefix = GetJSONObject(jsonapi, "subentity_prefix", Json::ValueType::stringValue, &empty_string);

    json_api_.GetURIs().SetBase(prefix.asString());
    json_api_.SetUserId(user_id.asString());
    json_api_.SetEntityId(entity_id.asString());
    json_api_.SetEntitySchema(entity_schema.asString());
    json_api_.SetShardedSchema(sharded_schema.asString());
    json_api_.SetSubentitySchema(subentity_schema.asString());
    json_api_.SetSubentityPrefix(subentity_prefix.asString());
}


/**
 * @brief Fill a 'completed' response.
 *
 * @param o_response
 */
void ev::loop::beanstalkd::Job::SetCompletedResponse (Json::Value& o_response)
{
    o_response                            = Json::Value(Json::ValueType::objectValue);
    o_response["action"]                  = "response";
    o_response["content_type"]            = "application/json; charset=utf-8";
    o_response["response"]                = Json::Value(Json::ValueType::objectValue);
    o_response["response"]["success"]     = true;
    o_response["status"]                  = "completed";
}

/**
 * @brief Fill a 'cancelled' response.
 *
 * @param a_payload
 * @param o_response
 */
void ev::loop::beanstalkd::Job::SetCancelledResponse (const Json::Value& a_payload, Json::Value& o_response)
{
    o_response                 = Json::Value(Json::ValueType::objectValue);
    o_response["action"]       = "response";
    o_response["content_type"] = "application/json; charset=utf-8";
    o_response["response"]     = a_payload;
    o_response["status"]       = "cancelled";
}

/**
 * @brief Fill a 'failed' response.
 *
 * @param a_code
 * @param o_response
 */
void ev::loop::beanstalkd::Job::SetFailedResponse (uint16_t a_code, Json::Value& o_response)
{
    SetFailedResponse(a_code, errors_array_, o_response);
}

/**
 * @brief Fill a 'failed' response.
 *
 * @param a_code
 * @param a_payload
 * @param o_response
 */
void ev::loop::beanstalkd::Job::SetFailedResponse (uint16_t a_code, const Json::Value& a_payload, Json::Value& o_response)
{
    o_response                 = Json::Value(Json::ValueType::objectValue);
    o_response["action"]       = "response";
    o_response["content_type"] = "application/json; charset=utf-8";
    o_response["response"]     = a_payload;
    o_response["status"]       = "failed";
    o_response["status_code"]  = a_code;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Publish a 'job progress' message.
 *
 * @param a_payload
 */
void ev::loop::beanstalkd::Job::PublishProgress (const Json::Value& a_payload)
{
    Publish(a_payload);
}

/**
 * @brief Append an error.
 *
 * @param a_error
 */
void ev::loop::beanstalkd::Job::AppendError (const Json::Value& a_error)
{
    errors_array_.append(a_error);
}

/**
 * @brief Append an error.
 *
 * @param a_type
 * @param a_why
 * @param a_where
 * @param a_code
 */
void ev::loop::beanstalkd::Job::AppendError (const char* const a_type, const std::string& a_why, const char *const a_where, const int a_code)
{
    Json::Value error_object = Json::Value(Json::ValueType::objectValue);
    error_object["type"]     = a_type;
    error_object["why"]      = a_why;
    error_object["where"]    = a_where;
    error_object["code"]     = a_code;
    errors_array_.append(error_object);
}

/**
 * @brief Publish a 'job finished' message to 'job-signals' channel.
 *
 * @param a_status
 */
void ev::loop::beanstalkd::Job::Broadcast (const ev::loop::beanstalkd::Job::Status a_status)
{
    progress_       = Json::Value(Json::ValueType::objectValue);
    progress_["id"] = id_;
    switch (a_status) {
        case ev::loop::beanstalkd::Job::Status::Finished:
            progress_["status"] = "finished";
        break;
        case ev::loop::beanstalkd::Job::Status::Cancelled:
            progress_["status"] = "cancelled";
        break;
        default:
            throw ::ev::Exception("Broadcast status " UINT8_FMT " not implemented!", static_cast<uint8_t>(a_status));
    }
    progress_["channel"] = channel_;
    Publish(redis_signal_channel_, progress_);
}

/**
 * @brief Publish a response to the job channel and a 'job finished' message to 'job-signals' channel.
 *
 * @param a_respomse
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Finished (const Json::Value& a_response,
                                          const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    // ... publish response ...
    Publish(a_response, a_success_callback, a_failure_callback);
    
    // ... reset throttle timer ...
    progress_report_.last_tp_ = std::chrono::steady_clock::time_point::max();
    
    // ... broadcast job finished ...
    Broadcast(ev::loop::beanstalkd::Job::Status::Finished);
}

/**
 * @brief Publish a 'progress' message.
 *
 * @param a_progress
 */
void ev::loop::beanstalkd::Job::Publish (const ev::loop::beanstalkd::Job::Progress& a_progress)
{

    const char* key;
    
    const auto now_tp = std::chrono::steady_clock::now();
    
    progress_ = Json::Value(Json::ValueType::objectValue);
    switch (a_progress.status_) {
        case ev::loop::beanstalkd::Job::Status::InProgress:
            key = a_progress.key_;
            progress_["status"] = "in-progress";
            break;
        case ev::loop::beanstalkd::Job::Status::Finished:
            key = a_progress.key_;
            progress_["status"] = "finished";
            break;
        case ev::loop::beanstalkd::Job::Status::Failed:
            key = a_progress.key_;
            progress_["status"] = "failed";
            break;
        case ev::loop::beanstalkd::Job::Status::Cancelled:
            key = "i18n_job_cancelled";
            progress_["status"] = "cancelled";
            break;
        default:
            throw ::ev::Exception("Broadcast status " UINT8_FMT " not implemented!", static_cast<uint8_t>(a_progress.status_));
    }
    
    if ( -1.0 != a_progress.value_ ) {
        progress_["progress"] = a_progress.value_;
    }
    
    Json::Value i18n_array = Json::Value(Json::ValueType::arrayValue);
    i18n_array.append(key);
    for ( auto arg : a_progress.args_ ) {
        Json::Value& object = i18n_array.append(Json::Value(Json::ValueType::objectValue));
        object[arg.first] = arg.second;
    }
    progress_["message"]  = i18n_array;

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now_tp - progress_report_.last_tp_).count();

    if ( std::chrono::steady_clock::time_point::max() == progress_report_.last_tp_ || elapsed >= progress_report_.timeout_in_sec_ || a_progress.now_ == true ) {
        progress_report_.last_tp_ = now_tp;
        Publish(progress_);
    }
}

/**
 * @brief Publish 'progress' messages.
 *
 * @param a_progress
 */
void ev::loop::beanstalkd::Job::Publish (const std::vector<ev::loop::beanstalkd::Job::Progress>& a_progress)
{
    for ( auto& p : a_progress ) {
        Publish(p);
    }
}

/**
 * @brief Publish a message using a REDIS channel.
 *
 * @param a_channel
 * @param a_object
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Publish (const std::string& a_channel, const Json::Value& a_object,
                                         const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    const std::string redis_message = json_writer_.write(a_object);

    osal::ConditionVariable cv;

    ExecuteOnMainThread([this, &a_channel, &a_success_callback, &a_failure_callback, &cv, &redis_message] {

        NewTask([this, &a_channel, &redis_message] () -> ::ev::Object* {

            return new ev::redis::Request(loggable_data_,
                                          "PUBLISH", { a_channel, redis_message }
            );

        })->Finally([&cv, a_success_callback] (::ev::Object* a_object) {

            // ... an integer reply is expected ...
            ev::redis::Reply::EnsureIntegerReply(a_object);

            // ... notify ...
            if ( nullptr != a_success_callback ) {
                a_success_callback();
            }

            cv.Wake();

        })->Catch([&cv, a_failure_callback] (const ::ev::Exception& a_ev_exception) {

            // ... notify ...
            if ( nullptr != a_failure_callback ) {
                a_failure_callback(a_ev_exception);
            }

            cv.Wake();

        });

    }, /* a_blocking */ false);

    cv.Wait();
}

/**
 * @brief Publish a message using a REDIS channel.
 *
 * @param a_object
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Publish (const Json::Value& a_object,
                                         const std::function<void()> a_success_callback,
                                         const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    osal::ConditionVariable cv;

    const std::string redis_channel = redis_channel_prefix_ + channel_;
    const std::string redis_key     = redis_key_prefix_     + channel_;
    const std::string redis_message = json_writer_.write(a_object);

    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("PUBLISH", "%s", redis_message.c_str());
    
    ExecuteOnMainThread([this, a_success_callback, a_failure_callback, &cv, &redis_channel, &redis_key, &redis_message] {

        ev::scheduler::Task* t = NewTask([this, &redis_channel, redis_message] () -> ::ev::Object* {

            return new ev::redis::Request(loggable_data_,
                                          "PUBLISH", { redis_channel, redis_message }
            );

        });

        if ( false == transient_ ) {

            t->Then([this, redis_key, redis_message] (::ev::Object* a_object) -> ::ev::Object* {

                // ... an integer reply is expected ...
                ev::redis::Reply::EnsureIntegerReply(a_object);

                // ... make it permanent ...
                return new ev::redis::Request(loggable_data_,
                                              "HSET",
                                              {
                                                  /* key   */ redis_key,
                                                  /* field */ "status", redis_message
                                              }
                );

            });

            if ( -1 != validity_ ) {

                if ( validity_ > 0 ) {

                    t->Then([this, redis_key] (::ev::Object* a_object) -> ::ev::Object* {

                        // ... a string 'OK' is expected ...
                        ev::redis::Reply::EnsureIntegerReply(a_object);

                        // ... set expiration date ...
                        return new ::ev::redis::Request(loggable_data_,
                                                        "EXPIRE", { redis_key, std::to_string(validity_) }
                        );

                    })->Finally([&cv, a_success_callback] (::ev::Object* a_object) {

                        //
                        // EXPIRE:
                        //
                        // Integer reply, specifically:
                        // - 1 if the timeout was set.
                        // - 0 if key does not exist or the timeout could not be set.
                        //
                        ev::redis::Reply::EnsureIntegerReply(a_object, 1);

                        // ... notify ...
                        if ( nullptr != a_success_callback ) {
                            a_success_callback();
                        }

                        cv.Wake();

                    });

                } else {

                    t->Finally([&cv, a_success_callback] (::ev::Object* a_object) {

                        // ... a string 'OK' is expected ...
                        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");

                        // ... notify ...
                        if ( nullptr != a_success_callback ) {
                            a_success_callback();
                        }

                        cv.Wake();

                    });

                }


            } else {

                // ... no validity set, validate 'SET' response ...
                t->Finally([&cv, a_success_callback] (::ev::Object* a_object) {

                    // ... an integer reply is expected ...
                    ev::redis::Reply::EnsureIntegerReply(a_object);

                    // ... notify ...
                    if ( nullptr != a_success_callback ) {
                        a_success_callback();
                    }

                    cv.Wake();

                });

            }

        } else {

            // ... transient, validate 'PUBLISH' response ...
            t->Finally([&cv, a_success_callback] (::ev::Object* a_object) {

                // ... an integer reply is expected ...
                ev::redis::Reply::EnsureIntegerReply(a_object);

                // ... notify ...
                if ( nullptr != a_success_callback ) {
                    a_success_callback();
                }

                cv.Wake();

            });

        }

        t->Catch([this, &cv, a_failure_callback] (const ::ev::Exception& a_ev_exception) {

            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "PUBLISH FAILED: %s",
                                            a_ev_exception.what()
            );

            // ... notify ...
            if ( nullptr != a_failure_callback ) {
                a_failure_callback(a_ev_exception);
            }

            cv.Wake();

        });

    }, /* a_blocking */ false);

    cv.Wait();
}


/**
 * @brief Get a job cancellation flag.
 */
bool ev::loop::beanstalkd::Job::ShouldCancel ()
{
    // ... first check cancellation flag ...
    const std::string redis_key = redis_key_prefix_ + channel_;
    
    osal::ConditionVariable cancellation_cv;
    
    ExecuteOnMainThread([this, &cancellation_cv, redis_key] {
        
        NewTask([this, redis_key] () -> ::ev::Object* {
            
            return new ev::redis::Request(loggable_data_,
                                          "HGET",
                                          { /* key   */ redis_key, /* field */ "cancelled" }
            );
            
        })->Finally([this, &cancellation_cv] (::ev::Object* a_object) {
            
            //
            // HGET:
            //
            // - String value is expected:
            //
            const ::ev::redis::Value& value = ev::redis::Reply::GetCommandReplyValue(a_object);
            if ( false == value.IsNil() && true == value.IsString() ) {
                cancelled_ = ( 0 == strcasecmp(value.String().c_str(), "true") );
            }
            
            cancellation_cv.Wake();
            
        })->Catch([this, &cancellation_cv] (const ::ev::Exception& a_ev_exception) {
            
            // ... log error ...
            EV_LOOP_BEANSTALK_JOB_LOG("error",
                                      "HGET failed: %s",
                                      a_ev_exception.what()
                                      );
            
            cancellation_cv.Wake();
            
        });
        
    }, /* a_blocking */ false);
    
    cancellation_cv.Wait();
    
    // ... done ...
    return cancelled_;
}

#ifdef __APPLE__
#pragma mark -
#pragma mark Follow up job
#endif

/**
 * @brief Append a 'follow up' job.
 */
Json::Value& ev::loop::beanstalkd::Job::AppendFollowUpJob ()
{
    return follow_up_jobs_.append(Json::Value(Json::ValueType::objectValue));
}

/**
 * @brief Submit previously appended follow up jobs.
 *
 * @return True on success, on failure errors are set.
 */
bool ev::loop::beanstalkd::Job::SubmitFollowUpJobs ()
{
    for ( Json::ArrayIndex idx = 0 ; idx < follow_up_jobs_.size() ; ++idx ) {
        
        const Json::Value& entry = follow_up_jobs_[idx];
        const Json::Value& job   = entry["job"];
                
        OSALITE_DEBUG_TRACE("job", "Job #" INT64_FMT " ~= submitting follow up job #" SIZET_FMT ":\n%s",
                            id_, static_cast<size_t>(idx + 1), entry.toStyledString().c_str()
        );
        
        SubmitFollowUpJob(static_cast<size_t>(idx + 1), job);
        
        if ( true == HasErrorsSet() ) {
            break;
        }
        
    }
    return ( false == HasErrorsSet() );
}

/**
 * @brief Submit a 'follow up' job.
 *
 * @param a_number
 * @param a_job
 */
void ev::loop::beanstalkd::Job::SubmitFollowUpJob (const size_t a_number, const Json::Value& a_job)
{
    // ... first check cancellation flag ...
    follow_up_payload_    = a_job;
    follow_up_id_key_     = config_.service_id_ + ":jobs:sequential_id";
    follow_up_tube_       = follow_up_payload_["tube"].asString();
    follow_up_key_        = ( config_.service_id_ + ":jobs:" + follow_up_tube_ + ':' );
    follow_up_expires_in_ = follow_up_payload_.get("validity", 3600).asInt64();
    follow_up_ttr_        = follow_up_payload_.get("ttr", 300).asUInt();
    
    osal::ConditionVariable cancellation_cv;
    
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("SUBMIT",
                                    "FOLLOW UP JOB # " SIZET_FMT ": %s, ttr set to " UINT32_FMT ", expires in " INT64_FMT,
                                    a_number, follow_up_tube_.c_str(), follow_up_ttr_, follow_up_expires_in_
    );
    
    ExecuteOnMainThread([this, a_number, &cancellation_cv] {
        
        NewTask([this] () -> ::ev::Object* {
            
            // ...  get new job id ...
            return new ::ev::redis::Request(loggable_data_, "INCR", {
                /* key   */ follow_up_id_key_
            });
            
        })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
            
            //
            // INCR:
            //
            // - An integer reply is expected:
            //
            //  - the value of key after the increment
            //
            const ::ev::redis::Value& value = ::ev::redis::Reply::EnsureIntegerReply(a_object);
            
            follow_up_payload_["id"] = std::to_string(value.Integer());
            
            // ... set job key ...
            follow_up_key_ += follow_up_payload_["id"].asString();
            
            // ... first, set queued status ...
            return new ::ev::redis::Request(loggable_data_, "HSET", {
                /* key   */ follow_up_key_,
                /* field */ "status", "{\"status\":\"queued\"}"
            });
            
        })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
            
            //
            // HSET:
            //
            // - An integer reply is expected:
            //
            //  - 1 if field is a new field in the hash and value was set.
            //  - 0 if field already exists in the hash and the value was updated.
            //
            (void)::ev::redis::Reply::EnsureIntegerReply(a_object);
            
            return new ::ev::redis::Request(loggable_data_, "EXPIRE", { follow_up_key_, std::to_string(follow_up_expires_in_) });
            
        })->Finally([this, a_number, &cancellation_cv] (::ev::Object* a_object) {
            
            //
            // EXPIRE:
            //
            // Integer reply, specifically:
            // - 1 if the timeout was set.
            // - 0 if key does not exist or the timeout could not be set.
            //
            ::ev::redis::Reply::EnsureIntegerReply(a_object, 1);
            
            try {
                
                const std::string payload = json_writer_.write(follow_up_payload_);
                
                SubmitJob(/* a_tube */ follow_up_tube_.c_str(), /* a_payload */ payload, /* a_ttr */ follow_up_ttr_);
                
                EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("SUBMITTED",
                                                "FOLLOW UP JOB # " SIZET_FMT ": %s",
                                                static_cast<size_t>(a_number),
                                                payload.c_str()
                );

            } catch (const ::Beanstalk::ConnectException& a_btc_exception) {
                throw ::ev::Exception("%s", a_btc_exception.what());
            }
            
            cancellation_cv.Wake();
            
        })->Catch([this, a_number, &cancellation_cv] (const ::ev::Exception& a_ev_exception) {
            
            const std::string prefix = "rror while submitting follow up job # " + std::to_string(a_number) + " to tube " + follow_up_tube_;
            
            // ... log error ...
            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "SUBMIT FAILED: e%s: %s",
                                            prefix.c_str(), a_ev_exception.what()
            );
            
            AppendError(/* a_type  */ "Beanstalk Error",
                        /* a_why   */ ( 'E'+ prefix + ": " + a_ev_exception.what() ).c_str(),
                        /* a_where */__FUNCTION__,
                        /* a_code */ ev::loop::beanstalkd::Job::k_exception_rc_
            );
            
            cancellation_cv.Wake();
            
        });
        
    }, /* a_blocking */ false);
    
    cancellation_cv.Wait();
}

#ifdef __APPLE__
#pragma mark -
#pragma mark Threading
#endif

/**
 * @brief Execute a callback on main thread.
 *
 * @param a_callback
 * @param a_blocking
 */
void ev::loop::beanstalkd::Job::ExecuteOnMainThread (std::function<void()> a_callback, bool a_blocking)
{
    callbacks_ptr_->on_main_thread_(a_callback, a_blocking);
}


/**
 * @brief Report a faltal exception.
 *
 * @param a_exception
 */
void ev::loop::beanstalkd::Job::OnFatalException (const ev::Exception& a_exception)
{
    callbacks_ptr_->on_fatal_exception_(a_exception);
}

#ifdef __APPLE__
#pragma mark -
#pragma mark Beanstalk.
#endif

/**
 * @brief Submit a 'beanstalkd' job.
 *
 * @param a_tube
 * @param a_payload
 * @param a_ttr
 */
void ev::loop::beanstalkd::Job::SubmitJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)
{
    callbacks_ptr_->on_submit_job_(a_tube, a_payload, a_ttr);
}

#ifdef __APPLE__
#pragma mark -
#pragma mark JSONAPI
#endif

/**
 * @brief Set JSON API configuration.
 *
 * @param a_config
 */
void ev::loop::beanstalkd::Job::SetJSONAPIConfig (const Json::Value& a_config)
{
    const Json::Value empty_string     = "";
    const Json::Value prefix           = GetJSONObject(a_config, "prefix"          , Json::ValueType::stringValue, &empty_string);
    const Json::Value user_id          = GetJSONObject(a_config, "user_id"         , Json::ValueType::stringValue, &empty_string);
    const Json::Value entity_id        = GetJSONObject(a_config, "entity_id"       , Json::ValueType::stringValue, &empty_string);
    const Json::Value entity_schema    = GetJSONObject(a_config, "entity_schema"   , Json::ValueType::stringValue, &empty_string);
    const Json::Value sharded_schema   = GetJSONObject(a_config, "sharded_schema"  , Json::ValueType::stringValue, &empty_string);
    const Json::Value subentity_schema = GetJSONObject(a_config, "subentity_schema", Json::ValueType::stringValue, &empty_string);
    const Json::Value subentity_prefix = GetJSONObject(a_config, "subentity_prefix", Json::ValueType::stringValue, &empty_string);
    
    json_api_.GetURIs().SetBase(prefix.asString());
    json_api_.SetUserId(user_id.asString());
    json_api_.SetEntityId(entity_id.asString());
    json_api_.SetEntitySchema(entity_schema.asString());
    json_api_.SetShardedSchema(sharded_schema.asString());
    json_api_.SetSubentitySchema(subentity_schema.asString());
    json_api_.SetSubentityPrefix(subentity_prefix.asString());
}

/**
 * @brief Perform a JSON API request.
 *
 * @param a_urn
 * @param o_code
 * @param o_data
 * @param o_elapsed
 * @param o_query
 */
void ev::loop::beanstalkd::Job::JSONAPIGet (const Json::Value& a_urn,
                                            uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_query)
{
    const std::string url = json_api_.GetURIs().GetBase() + a_urn.asString();
    
    osal::ConditionVariable cv;
    
    o_query = "";
    o_code  = 500;
    o_data  = "";
    
    ExecuteOnMainThread([this, &url, &cv, &o_code, &o_data, &o_query, &o_elapsed] {
        
        json_api_.Get(/* a_loggable_data */
                      loggable_data_,
                      /* url */
                      url,
                      /* a_callback */
                      [this, &o_code, &o_data, &o_query, &o_elapsed, &cv] (const char* /* a_uri */, const char* a_json, const char* /* a_error */, uint16_t a_status, uint64_t a_elapsed) {
                          o_code    = a_status;
                          o_data    = ( nullptr != a_json ? a_json : "" );
                          o_elapsed = a_elapsed;
                          cv.Wake();
                      },
                      &o_query
        );
        
    }, /* a_blocking */ false);
    
    cv.Wait();
}

#ifdef __APPLE__
#pragma mark -
#pragma mark HTTP
#endif

/**
 * @brief Perform an HTTP request.
 *
 * @param a_url
 * @param o_code
 * @param o_data
 * @param o_elapsed
 * @param o_url
 */
void ev::loop::beanstalkd::Job::HTTPGet (const Json::Value& a_url,
                                         uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_url)
{
    osal::ConditionVariable cv;
    
    const auto dlsp = std::chrono::steady_clock::now();
    
    o_url  = a_url.asString();
    o_data = "";
    o_code = 500;

    ExecuteOnMainThread([this, &cv, &o_url, &o_code, &o_data, &o_elapsed, &dlsp] {
        
        http_.GET(
                  loggable_data_,
                  /* a_url */
                  o_url,
                  /* a_headers */
                  nullptr,
                  /* a_success_callback */
                  [this, &o_code, &o_data, &o_elapsed, &cv](const ::ev::curl::Value& a_value) {
                      o_code    = a_value.code();
                      o_data    = a_value.body();
                      cv.Wake();
                  },
                  /* a_failure_callback */
                  [this, &o_code, &o_data, &o_elapsed, &cv](const ::ev::Exception& a_exception) {
                      o_code    = 500;
                      o_data    = a_exception.what();
                      cv.Wake();
                  }
        );
        
    }, /* a_blocking */ false);

    o_elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - dlsp).count());

    cv.Wait();
}

#ifdef __APPLE__
#pragma mark -
#pragma mark FILE
#endif

/**
 * @brief Load a file data to an \link std::string \link.
 *
 * @param a_uri
 * @param o_code
 * @param o_data
 * @param o_elapsed
 * @param o_uri
 */
void ev::loop::beanstalkd::Job::LoadFile (const Json::Value& a_uri,
                                          uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_uri)
{
    const auto dlsp = std::chrono::steady_clock::now();

    o_uri = a_uri.asString();
    
    if ( false == osal::File::Exists(o_uri.c_str()) ) {
        o_data = "";
        o_code = 404;
    } else {
        o_data = "";
        o_code = 500;
        
        std::ifstream in_stream(o_uri, std::ifstream::in);
        
        o_data = std::string((std::istreambuf_iterator<char>(in_stream)), std::istreambuf_iterator<char>());
        o_code = 200;
    }
    
    o_elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - dlsp).count());
}

#ifdef __APPLE__
#pragma mark -
#pragma mark PostgreSQL
#endif

/**
 * @brief Execute an SQL query.
 *
 * @param a_query
 * @param o_result
 * @param a_use_column_name
 */
void ev::loop::beanstalkd::Job::ExecuteQuery (const std::string& a_query, Json::Value& o_result, const bool a_use_column_name = false)
{
    o_result = Json::Value(Json::ValueType::objectValue);
    o_result["status_code"] = 500;

    osal::ConditionVariable cv;

    ExecuteOnMainThread([this, &a_query, &o_result, &cv, &a_use_column_name] () {

        NewTask([this, a_query] () -> ::ev::Object* {

            return new ::ev::postgresql::Request(loggable_data_, a_query);

        })->Then([] (::ev::Object* a_object) -> ::ev::Object* {

            ::ev::Result* result = dynamic_cast<::ev::Result*>(a_object);
            if ( nullptr == result ) {
                throw ::ev::Exception("Unexpected PostgreSQL result object: nullptr!");
            }

            if ( 1 != result->DataObjectsCount() ) {
                throw ::ev::Exception("Unexpected number of PostgreSQL result objects: got " SIZET_FMT", expecting " SIZET_FMT "!",
                                      result->DataObjectsCount(), static_cast<size_t>(1)
                );
            }

            const ::ev::postgresql::Reply* reply = dynamic_cast<const ::ev::postgresql::Reply*>(result->DataObject());
            if ( nullptr == reply ) {
                const ::ev::postgresql::Error* error = dynamic_cast<const ::ev::postgresql::Error*>(result->DataObject());
                if ( nullptr != error ) {
                    std::string msg = error->message();
                    for ( char c : { '\\', '\b', '\f', '\r', '\n', '\t' } ) {
                        msg.erase(std::remove(msg.begin(), msg.end(), c), msg.end());
                    }
                    throw ::ev::Exception(msg);
                } else {
                    throw ::ev::Exception("Unexpected PostgreSQL reply object: nullptr!");
                }
            }

            // ... same as reply, but it was detached ...
            return result->DetachDataObject();

        })->Finally([a_query, &o_result, &cv, &a_use_column_name] (::ev::Object* a_object) {

            const ::ev::postgresql::Reply* reply = dynamic_cast<const ::ev::postgresql::Reply*>(a_object);
            if ( nullptr == reply ) {
                throw ::ev::Exception("Unexpected PostgreSQL data object!");
            }

            const ::ev::postgresql::Value& value = reply->value();

            if ( true == value.is_error() ) {

                const char* const error = value.error_message();
                throw ::ev::Exception("PostgreSQL error: '%s'!", nullptr != error ? error : "nullptr");

            } else if ( true == value.is_null() ) {
                throw ::ev::Exception("Unexpected PostgreSQL unexpected data object : null!");
            }

            const int rows_count    = value.rows_count();
            const int columns_count = value.columns_count();

            o_result["table"] = Json::Value(Json::ValueType::arrayValue);
            for ( int row_idx = 0 ; row_idx < rows_count ; ++row_idx ) {
                Json::Value& line = o_result["table"].append(Json::Value(Json::ValueType::objectValue));
                for ( int column_idx = 0 ; column_idx < columns_count ; ++column_idx ) {
                    if ( true == a_use_column_name ) {
                        line[value.column_name(column_idx)] = value.raw_value(row_idx, column_idx);
                    } else {
                        line[std::to_string(column_idx)] = value.raw_value(row_idx, column_idx);
                    }
                }
            }

            o_result["status_code"] = 200;

            cv.Wake();

        })->Catch([a_query, &o_result, &cv] (const ::ev::Exception& a_ev_exception) {

            o_result["status_code"] = 500;
            o_result["exception"]   = a_ev_exception.what();

            cv.Wake();

        });

    }, /* a_blocking */ false);

    cv.Wait();
}

/**
 * @brief Execute an SQL query.
 *
 * @param a_query
 * @param o_result
 */

void ev::loop::beanstalkd::Job::ExecuteQueryWithJSONAPI (const std::string& a_query, Json::Value& o_result)
{
    o_result = Json::Value(Json::ValueType::objectValue);
    o_result["status_code"] = 500;

    osal::ConditionVariable cv;

    json_api_.Get(/* a_uri */
                  "",
                  /* a_callback */
                  [&o_result, &cv](const char* /* a_uri */, const char* a_json, const char* /* a_error */, uint16_t a_status, uint64_t /* a_elapsed */) {

                      o_result["reponse"]     = a_json; // TODO PARSE;
                      o_result["status_code"] = a_status;

                      cv.Wake();
                  }
    );

    cv.Wait();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Ensure output directory is created.
 *
 * @a_validity
 */
const std::string& ev::loop::beanstalkd::Job::EnsureOutputDir (const int64_t a_validity)
{
    // ... get time w/offset ...
    cc::UTCTime::HumanReadable now_hrt = cc::UTCTime::ToHumanReadable(cc::UTCTime::OffsetBy(-1 != a_validity ? a_validity : validity_));
    
    // ... check if output directory must change ...
    if ( 0 == output_directory_.length() || now_hrt.year_ != chdir_hrt_.year_ || now_hrt.month_ != chdir_hrt_.month_ || now_hrt.day_ !=  chdir_hrt_.day_ ) {
        
        // ... setup new output directory ...
        hrt_buffer_[0] = '\0';
        const int w = snprintf(hrt_buffer_, 26, "%04d-%02d-%02d/",
                               static_cast<int>(now_hrt.year_), static_cast<int>(now_hrt.month_), static_cast<int>(now_hrt.day_)
                               );
        if ( w < 0 || w > 26 ) {
            throw ev::Exception("Unable to change output directory - buffer write error!");
        }
        output_directory_ = output_directory_prefix_ + hrt_buffer_;
        
        if ( osal::Dir::EStatusOk != osal::Dir::CreateDir(output_directory_.c_str()) ) {
            throw ev::Exception("Unable to create output directory %s!", output_directory_.c_str());
        }
        
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("SETUP", "Changing output dir to %s...",
                                        output_directory_.c_str()
        );
        
        // ... prevent unnecessary changes ...
        chdir_hrt_ = now_hrt;
    } else if ( output_directory_.length() > 0 && osal::Dir::EStatusOk != osal::Dir::CreateDir(output_directory_.c_str()) ) {
        throw ev::Exception("Unable to create output directory %s!", output_directory_.c_str());
    }
    
    // ... sanity check ...
    if ( 0 == output_directory_.length() ) {
        throw ev::Exception("Unable to change output directory - not set!");
    }
    
    // ... done ...
    return output_directory_;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Retrieve a JSON object or it's to a default value.
 *
 * @param a_param
 * @param a_key
 * @param a_type
 * @param a_default
 *
 * @return
 */
Json::Value ev::loop::beanstalkd::Job::GetJSONObject (const Json::Value& a_parent, const char* const a_key,
                                                      const Json::ValueType& a_type, const Json::Value* a_default)
{
    std::stringstream tmp_ss;

    Json::Value value = a_parent.get(a_key, Json::nullValue);
    if ( true == value.isNull() ) {
        if ( nullptr != a_default ) {
            return *a_default;
        } else if ( Json::ValueType::nullValue == a_type ) {
            return value;
        } /* else { } */
    } else if ( value.type() == a_type ) {
        return value;
    }

    tmp_ss << "Error while retrieving JSON object named '" << a_key <<"' - type mismatch: got " << value.type() << ", expected " << a_type << "!";
    throw std::runtime_error(tmp_ss.str());
}

#ifdef __APPLE__
#pragma mark -
#pragma mark [Protected] - Scheduler
#endif

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
ev::scheduler::Task* ev::loop::beanstalkd::Job::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ev::scheduler::Task(a_callback,
                                   [this](::ev::scheduler::Task* a_task) {
                                       ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                   }
    );
}

#ifdef __APPLE__
#pragma mark -
#pragma mark [Private] - REDIS Callback
#endif

/**
 * @brief Called by REDIS subscriptions manager.
 *
 * @param a_name
 * @param a_message
 */
EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK ev::loop::beanstalkd::Job::JobSignalsDataCallback (const std::string& a_name, const std::string& a_message)
{
    ExecuteOnMainThread([this, a_name, a_message] () {

        Json::Value  object;
        Json::Reader reader;
        try {
            if ( true == reader.parse(a_message, object, false) ) {
                if ( true ==  object.isMember("id") && true == object.isMember("status") ) {
                    const Json::Value id = object.get("id", -1);
                    if ( 0 == channel_.compare(std::to_string(static_cast<int64_t>(id.asInt64()))) ) {
                        const Json::Value value = object.get("status", Json::Value::null);
                        if ( true == value.isString() && 0 == strcasecmp(value.asCString(), "cancelled") ) {
                            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("CANCELLED", "%s: %s",
                                                            a_name.c_str(), a_message.c_str()
                            );
                            cancelled_ = true;
                        }
                    }
                }
            }
        } catch (const Json::Exception& /* a_json_exception */) {
            // ... eat it ...
        }

    }, /* a_blocking */ false);

    return nullptr;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Called when a REDIS connection has been lost.
 */
void ev::loop::beanstalkd::Job::OnREDISConnectionLost ()
{
    OnFatalException(ev::Exception("REDIS connection lost:\n unable to reconnect to REDIS!"));
}
