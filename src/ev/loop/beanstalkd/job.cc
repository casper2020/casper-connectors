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

#include "cc/debug/types.h"
#include "cc/macros.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_tube
 * @param a_config
 * @param a_response_flags
 */
ev::loop::beanstalkd::Job::Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Job::Config& a_config,
                                const Job::ResponseFlags& a_response_flags)
    : ::ev::loop::beanstalkd::Object(a_loggable_data),
    tube_(a_tube), config_(a_config),
    redis_signal_channel_(config_.service_id() + ":job-signal"),
    redis_key_prefix_(config_.service_id() + ":jobs:" + tube_ + ':'),
    redis_channel_prefix_(config_.service_id() + ':' + tube_ + ':'),
    default_validity_(3600),
    json_api_(loggable_data_, /* a_enable_task_cancellation */ false)
{
    bjid_                            = 0;
    validity_                        = -1;
    transient_                       = config_.transient();
    progress_report_.timeout_in_sec_ = config_.min_progress();
    progress_report_.last_tp_        = std::chrono::steady_clock::time_point::max();
    cancelled_                       = false;
    already_ran_                     = false;
    deferred_                        = false;

    progress_                        = Json::Value::null;

    chdir_hrt_.seconds_              = static_cast<uint8_t>(0);
    chdir_hrt_.minutes_              = static_cast<uint8_t>(0);
    chdir_hrt_.hours_                = static_cast<uint8_t>(0);
    chdir_hrt_.day_                  = static_cast<uint8_t>(0);
    chdir_hrt_.month_                = static_cast<uint8_t>(0);
    chdir_hrt_.year_                 = static_cast<uint16_t>(0);
    
    hrt_buffer_[0]                   = '\0';
       
    json_writer_.omitEndingLineFeed();
    
    callbacks_ptr_ = nullptr;
    start_tp_      = std::chrono::steady_clock::time_point::min();
    end_tp_        = std::chrono::steady_clock::time_point::min();
    
    signals_channel_listener_ = nullptr;
    owner_log_callback_       = nullptr;
    
    response_flags_           = a_response_flags;
    
    ev::LoggerV2::GetInstance().Register(logger_client_, { "queue" });
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Job::~Job ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief One-shot setup.
 *
 * @param a_callbacks
 * @param a_shared_config
 */
void ev::loop::beanstalkd::Job::Setup (const Job::MessagePumpCallbacks* a_callbacks,
                                       const ::ev::loop::beanstalkd::SharedConfig& a_shared_config)
{
    osal::ConditionVariable cv;
    
    callbacks_ptr_           = a_callbacks;
    output_directory_prefix_ = a_shared_config.directories_.output_;
    output_directory_        = "";
    logs_directory_          = a_shared_config.directories_.log_;
    shared_directory_        = a_shared_config.directories_.shared_;

    ExecuteOnMainThread([this, &cv] {

        ::ev::scheduler::Scheduler::GetInstance().Register(this);

        ::ev::redis::subscriptions::Manager::GetInstance().SubscribeChannels({ redis_signal_channel_ },
                                                                             /* a_status_callback */
                                                                             [&cv](const std::string& a_name_or_pattern,
                                                                                         const ::ev::redis::subscriptions::Manager::Status& a_status) -> EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK {
                                                                                 if ( ::ev::redis::subscriptions::Manager::Status::Subscribed == a_status ) {
                                                                                     cv.Wake();
                                                                                 }
                                                                                 return nullptr;
                                                                             },
                                                                             /* a_data_callback */
                                                                             std::bind(&ev::loop::beanstalkd::Job::OnSignalsChannelMessageReceived, this, std::placeholders::_1, std::placeholders::_2),
                                                                             /* a_client */
                                                                             this
        );
    }, /* a_blocking */ false);
    cv.Wait();
    
    // ... setup job ...
    Setup();
}

/**
* @brief One-shot dismantling.
*
* @param a_cc_exception
*/
void ev::loop::beanstalkd::Job::Dismantle (const ::cc::Exception* /* a_cc_exception */)
{
    ExecuteOnMainThread([this] {
        ::ev::redis::subscriptions::Manager::GetInstance().Unubscribe(this);
        ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
    }, /* a_blocking */ true);
    Dismantle();
}

/**
 * @brief Prepare a job to run.
 *
 * @param a_id
 * @param a_payload
 * @param a_completed_callback
 * @param a_cancelled_callback
 * @param a_deferred_callback
 */
void ev::loop::beanstalkd::Job::Consume (const int64_t& a_id, const Json::Value& a_payload,
                                         const ev::loop::beanstalkd::Job::CompletedCallback& a_completed_callback,
                                         const ev::loop::beanstalkd::Job::CancelledCallback& a_cancelled_callback,
                                         const ev::loop::beanstalkd::Job::DeferredCallback& a_deferred_callback)
{
    //
    // JOB Configuration - ID is mandatory
    //
    const Json::Value redis_channel = a_payload.get("id", Json::nullValue); // id: a.k.a. REDIS JOB ID ( numeric id )
    if ( true == redis_channel.isNull() || false == redis_channel.isString() ||  0 == redis_channel.asString().length() ) {
        throw ev::Exception("Missing or invalid 'id' object!");
    }

    //
    // RESET 'job' variables
    //
    bjid_                            = a_id; // beanstalkd number id
    rjnr_                            = redis_channel.asString(); // <name>:<redis numeric id>
    rjid_                            = ( redis_key_prefix_ + rjnr_ );
    rcid_                            = ( redis_channel_prefix_ + rjnr_ );
    validity_                        = a_payload.get("validity" , default_validity_).asInt64();
    transient_                       = a_payload.get("transient", config_.transient()).asBool();
    progress_report_.timeout_in_sec_ = a_payload.get("min_progress", config_.min_progress()).asInt();
    cancelled_                       = false;
    already_ran_                     = false;
    deferred_                        = false;
    progress_                        = Json::Value(Json::ValueType::objectValue);
    
    //
    // JSONAPI Configuration - optional
    //
    if ( true == a_payload.isMember("jsonapi") ) {
        ConfigJSONAPI(a_payload["jsonapi"]);
    }

    //
    // Configure Log
    //
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), tube_ + ':' + rjnr_);
    
    //
    // Check for job cancellation
    //
    const auto cancelled_callback = [this, &a_cancelled_callback] (bool a_already_ran) {
      
        // ... publish 'job cancelled' 'progress' ...
        Publish({
            /* key_    */ nullptr,
            /* args_   */ {},
            /* status_ */ ev::loop::beanstalkd::Job::Status::Cancelled,
            /* value_  */ -1.0
        });
        
        a_cancelled_callback(a_already_ran);
        
    };
    
    start_tp_ = std::chrono::steady_clock::now();
    end_tp_   = start_tp_;

    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("PAYLOAD", "%s", json_writer_.write(a_payload).c_str());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("TUBE"   , "%s", tube_.c_str());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ID"     , "%s", a_payload.get("id"  , "???").asCString());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("CHANNEL", "%s", rcid_.c_str());
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("KEY"    , "%s", rjid_.c_str());

    // ... first check if job as cancelled ...
    if ( true == ShouldCancel() ) {
        // ... notify caller ...
        cancelled_callback(already_ran_);
    } else {
        // ... execute job ...
        Run(a_id, a_payload, a_completed_callback, cancelled_callback, a_deferred_callback);
    }

    end_tp_ = std::chrono::steady_clock::now();
    
    EV_LOOP_BEANSTALK_JOB_LOG_DEFINE_CONST(auto, elapsed, std::chrono::duration_cast<std::chrono::milliseconds>(end_tp_ - start_tp()).count());
    
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("DONE", "" UINT64_FMT "ms", static_cast<uint64_t>(elapsed));

    // ... prevent from REDIS cancellation messages to be logged ...
    rjnr_  = "";
    rjid_  = "";
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

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Fill a 'response' object.
 *
 * param a_code
 * param a_status
 * param a_response
 * param o_object
 * param a_action
*/
uint16_t ev::loop::beanstalkd::Job::FillResponseObject (const uint16_t& a_code, const char* const a_status, const Json::Value& a_response,
                                                        Json::Value& o_object,
                                                        const char* const a_action)  const
{
    o_object           = Json::Value(Json::ValueType::objectValue);
    o_object[a_action] = Json::Value(Json::ValueType::objectValue);
    // ... merge?
    if ( true == a_response.isObject() ) {
        // ... yes ...
        MergeJSONValue(o_object[a_action], a_response);
        if ( true == o_object[a_action].isMember("message") ) {
            o_object["message"] = o_object[a_action].removeMember("message");
        }
    } else if ( false == a_response.isNull() ) {
        // ... 'response' is direct copy ...
        throw ::ev::Exception("%s", "Response must be a valid JSON object!");
    }
    
    // ... set / override mandatory fields ...
    if ( true == o_object.isMember(a_action) && true == o_object.isObject()
        && Job::ResponseFlags::SuccessFlag == ( response_flags_ & Job::ResponseFlags::SuccessFlag )
    ) {
        o_object[a_action]["success"] = ( 200 == a_code );
    }    
    o_object["action"]       = a_action;
    o_object["content_type"] = "application/json; charset=utf-8";
    o_object["status"]       = a_status;
    o_object["status_code"]  = a_code;
    // ... done ..
    return a_code;
}

/**
 * @brief Fill a 'completed' response.
 *
 * @param o_response
 */
uint16_t ev::loop::beanstalkd::Job::SetCompletedResponse (Json::Value& o_response)  const
{
    return FillResponseObject(200, "completed",  Json::Value::null, o_response);
}

/**
 * @brief Fill a 'completed' response.
 *
 * @param a_payload
 * @param o_response
 */
uint16_t ev::loop::beanstalkd::Job::SetCompletedResponse (const Json::Value& a_payload, Json::Value& o_response)  const
{
    return FillResponseObject(200, "completed",  a_payload, o_response);
}

/**
 * @brief Fill a 'cancelled' response.
 *
 * @param a_payload
 * @param o_response
 */
void ev::loop::beanstalkd::Job::SetCancelledResponse (const Json::Value& a_payload, Json::Value& o_response)  const
{
    (void)FillResponseObject(200, "cancelled",  a_payload, o_response);
}

/**
 * @brief Fill a 'redirect' response.
 *
 * @param a_payload
 * @param o_response
 * @param o_code Alternative 30x code.
*/
uint16_t ev::loop::beanstalkd::Job::SetRedirectResponse (const Json::Value& a_payload, Json::Value& o_response, const uint16_t a_code) const
{
    return FillResponseObject(a_code, "completed",  a_payload, o_response, "redirect");
}

/**
 * @brief Fill a 'bad request' response ( 400 - Bad Request ).
 *
 * @param a_why
 * @param o_response
 *
 * @return HTTP status code.
 */
uint16_t ev::loop::beanstalkd::Job::SetBadRequestResponse (const std::string& a_why, Json::Value& o_response)  const
{
    return SetFailedResponse(400, Json::Value::null, o_response);
}

/**
 * @brief Fill a 'timeout' response ( 408 - Request Timeout ).
 *
 * @param a_payload
 * @param o_response
 *
 * @return HTTP status code.
 */
uint16_t ev::loop::beanstalkd::Job::SetTimeoutResponse (const Json::Value& a_payload, Json::Value& o_response) const
{
    return SetFailedResponse(408, a_payload, o_response);
}

/**
 * @brief Fill a 'internal server error' response ( 500 - Internal Server Error ).
 *
 * @param a_why
 * @param o_response
 *
 * @return HTTP status code.
 */
uint16_t ev::loop::beanstalkd::Job::SetInternalServerErrorResponse (const std::string& a_why, Json::Value& o_response) const
{
    return SetFailedResponse(500, Json::Value::null, o_response);
}

/**
 * @brief Fill a 'not implemented' response ( 501 - Not Implemented ).
 *
 * @param a_payload
 * @param o_response
 *
 * @return HTTP status code.
 */
uint16_t ev::loop::beanstalkd::Job::SetNotImplementedResponse (const Json::Value& a_payload, Json::Value& o_response) const
{
    return SetFailedResponse(501, a_payload, o_response);
}

/**
 * @brief Fill a 'failed' response.
 *
 * @param a_code
 * @param a_payload
 * @param o_response
 */
uint16_t ev::loop::beanstalkd::Job::SetFailedResponse (const uint16_t& a_code, const Json::Value& a_payload, Json::Value& o_response) const
{
    return FillResponseObject(a_code, "failed",  a_payload, o_response);
}

/**
 * @brief Fill a 'failed' response.
 *
 * @param a_code
 * @param o_response
 */
uint16_t ev::loop::beanstalkd::Job::SetFailedResponse (const uint16_t&  a_code, Json::Value& o_response) const
{
    return SetFailedResponse(a_code, Json::Value::null, o_response);
}

// MARK: -

void ev::loop::beanstalkd::Job::SetProgress (const ev::loop::beanstalkd::Job::Progress& a_progress, Json::Value& o_progress) const
{
    const char* key;
    
    o_progress = Json::Value(Json::ValueType::objectValue);
    switch (a_progress.status_) {
        case ev::loop::beanstalkd::Job::Status::InProgress:
            key = a_progress.key_;
            o_progress["status"] = "in-progress";
            break;
        case ev::loop::beanstalkd::Job::Status::Finished:
            key = a_progress.key_;
            o_progress["status"] = "finished";
            break;
        case ev::loop::beanstalkd::Job::Status::Failed:
            key = a_progress.key_;
            o_progress["status"] = "failed";
            break;
        case ev::loop::beanstalkd::Job::Status::Cancelled:
            key = "i18n_job_cancelled";
            o_progress["status"] = "cancelled";
            break;
        default:
            throw ::ev::Exception("Broadcast status " UINT8_FMT " not implemented!", static_cast<uint8_t>(a_progress.status_));
    }
    
    if ( -1.0 != a_progress.value_ ) {
        o_progress["progress"] = a_progress.value_;
    }
    
    Json::Value i18n_array = Json::Value(Json::ValueType::arrayValue);
    i18n_array.append(key);
    for ( auto arg : a_progress.args_ ) {
        Json::Value& object = i18n_array.append(Json::Value(Json::ValueType::objectValue));
        object[arg.first] = arg.second;
    }
    o_progress["message"]  = i18n_array;
}


// MARK: -

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
 * @brief Publish a response to the job channel and a 'job finished' message to 'job-signals' channel.
 *
 * @param a_response
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Finished (const Json::Value& a_response,
                                          const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{    
    uint64_t rjnr = 0;
    const char* ptr = strrchr(rjnr_.c_str(), ':');
    if ( nullptr != ptr ) {
        if ( 1 != sscanf(ptr + sizeof(char), UINT64_FMT, &rjnr) ) {
            throw ::ev::Exception("Unable to extract numeric ID from string '%s'!", rjnr_.c_str());
        }
    } else {
        if ( 1 != sscanf(rjnr_.c_str(), UINT64_FMT, &rjnr)  ) {
            throw ::ev::Exception("Unable to extract numeric ID from string '%s'!", rjnr_.c_str());
        }
    }
    Finished(rjnr, rcid_, a_response, a_success_callback, a_failure_callback);
}

/**
 * @brief Publish a 'message' to a specific channel
 *
 * @param a_fq_channel Fully qualified channel.
 * @param a_object     JSON object to publish.
 */
void ev::loop::beanstalkd::Job::Relay (const uint64_t& a_id, const std::string& a_fq_channel, const Json::Value& a_object)
{
    // ... reset throttle timer ...
    progress_report_.last_tp_ = std::chrono::steady_clock::time_point::max();

    // ... publish message ...
    Publish(a_id, a_fq_channel, a_object);
}

/**
 * @brief Publish a response to a specific channel and a 'job finished' message to 'job-signals' channel.
 *
 * @param a_id
 * @param a_fq_channel
 * @param a_response
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Finished (const uint64_t& a_id, const std::string& a_fq_channel, const Json::Value& a_response,
                                          const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    // ... publish response ...
    Publish(a_id, a_fq_channel, a_response, a_success_callback, a_failure_callback);
    
    // ... reset throttle timer ...
    progress_report_.last_tp_ = std::chrono::steady_clock::time_point::max();
    
    // ... broadcast job finished ...
    Broadcast(a_id, a_fq_channel, ev::loop::beanstalkd::Job::Status::Finished);
}

/**
 * @brief Publish a 'progress' message to a specific channel and a 'job finished' message to 'job-signals' channel.
 *
 * @param a_id
 * @param a_fq_channel
 * @param a_progress
 */
void ev::loop::beanstalkd::Job::Publish (const uint64_t& a_id, const std::string& a_fq_channel, const Progress& a_progress)
{
    SetProgress(a_progress, progress_);
    Publish(a_id, a_fq_channel, progress_);
}

/**
 * @brief Publish a 'status' message to 'job-signals' channel for a specific job channel.
 *
 * @param a_id
 * @param a_fq_channel
 * @param a_status
 */
void ev::loop::beanstalkd::Job::Broadcast (const uint64_t& a_id, const std::string& a_fq_channel, const ev::loop::beanstalkd::Job::Status a_status)
{
    progress_       = Json::Value(Json::ValueType::objectValue);
    progress_["id"] = a_id;
    
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
    progress_["channel"] = a_fq_channel;
    
    Publish(a_id, redis_signal_channel_, progress_);
}

/**
 * @brief Publish a 'cancelled' message to 'job-signals' channel for a specific job channel.
 *
 * @param a_id                BEANSTALKD job ID.
 * @param a_fq_channel        REDIS fully qualifiled channel ( <tube>:<id> ).
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Cancel (const uint64_t& a_id, const std::string& a_fq_channel,
                                        const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    osal::ConditionVariable cv;
    
    ::ev::Exception* exception = nullptr;
    
    ExecuteOnMainThread([this, &a_id, &a_fq_channel, &cv, &exception] () {
        NewTask([this, &a_fq_channel] () -> ::ev::Object* {
                        
            // ... save cancellation flag ...
            return new ::ev::redis::Request(loggable_data_, "HSET", {
                      /* key   */ a_fq_channel,
                      /* field */ "cancelled", "true"
            });
        })->Then([this, &a_id, &a_fq_channel] (::ev::Object* a_object) -> ::ev::Object* {
            //
            // HSET:
            //
            // - Integer reply is expected:
            //
            //  - 1 if field is a new field in the hash and value was set.
            //  - 0 if field already exists in the hash and the value was updated.
            //
            (void)ev::redis::Reply::EnsureIntegerReply(a_object);
            // ... broadcast message ...
            return new ::ev::redis::Request(loggable_data_, "PUBLISH", {
                /* channel */ redis_signal_channel_,
                /* message */ "{\"id\":" + std::to_string(a_id) + ",\"status\":\"cancelled\",\"channel\":\"" + a_fq_channel + "\"}"
            });
        })->Finally([&cv] (::ev::Object* a_object) {
            //
            // PUBLISH:
            //
            // - Integer reply is expected:
            //
            //  - the number of clients that received the message.
            //
            (void)ev::redis::Reply::EnsureIntegerReply(a_object);
            // ... WAKE from REDIS operations ...
            cv.Wake();
        })->Catch([&cv, &exception] (const ::ev::Exception& a_ev_exception) {
            // ... copy exception ...
            exception = new ::ev::Exception(a_ev_exception);
            // ... WAKE from REDIS operations ...
            cv.Wake();
        });
    }, /* a_blocking */ false);

    // ... WAIT until REDIS operations are completed ...
    cv.Wait();
    
    // ... an exception ocurred?
    if ( nullptr != exception ) {
        // ... notify?
        if ( nullptr != a_failure_callback ) {
            // ... copy ...
            const ::ev::Exception copy = ::ev::Exception(*exception);
            // ... release it ...
            delete exception;
            // ... notify ...
            a_failure_callback(copy);
        } else {
            // ... log it ...
            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "CANCEL FAILED: %s",
                                            exception->what()
            );
            // ... release it ...
            delete exception;
        }
    }
}

/**
 * @brief Publish a 'progress' message.
 *
 * @param a_progress
 */
void ev::loop::beanstalkd::Job::Publish (const ev::loop::beanstalkd::Job::Progress& a_progress)
{
    const auto now_tp = std::chrono::steady_clock::now();
    
    SetProgress(a_progress, progress_);

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
 * @param a_id
 * @param a_fq_channel
 * @param a_object
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Publish (const uint64_t& /* a_id */, const std::string& a_fq_channel,
                                         const Json::Value& a_object,
                                         const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    const std::string redis_message = json_writer_.write(a_object);

    osal::ConditionVariable cv;
    ev::Exception*          exception = nullptr;
    
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("PUBLISH", "%s", redis_message.c_str());

    ExecuteOnMainThread([this, &a_fq_channel, &cv, &redis_message, &exception] {

        NewTask([this, &a_fq_channel, &redis_message] () -> ::ev::Object* {
            // ... broadcast message ...
            return new ev::redis::Request(loggable_data_,
                                          "PUBLISH", { a_fq_channel, redis_message }
            );
        })->Finally([&cv] (::ev::Object* a_object) {
            //
            // PUBLISH:
            //
            // - Integer reply is expected:
            //
            //  - the number of clients that received the message.
            //
            ev::redis::Reply::EnsureIntegerReply(a_object);
            // ... WAKE from REDIS operations ...
            cv.Wake();
        })->Catch([&cv, &exception] (const ::ev::Exception& a_ev_exception) {
            // ... copy exception ...
            exception = new ev::Exception(a_ev_exception);
            // ... WAKE from REDIS operations ...
            cv.Wake();
        });

    }, /* a_blocking */ false);

    // ... WAIT until REDIS operations are completed ...
    cv.Wait();
    
    // ... an exception ocurred?
    if ( nullptr != exception ) {
        // ... notify?
        if ( nullptr != a_failure_callback ) {
            // ... copy ...
            const ::ev::Exception copy = ::ev::Exception(*exception);
            // ... release it ...
            delete exception;
            // ... notify ...
            a_failure_callback(copy);
        } else {
            // ... log it ...
            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "PUBLISH FAILED: %s",
                                            exception->what()
            );
            // ... release it ...
            delete exception;
        }
    } else if ( nullptr != a_success_callback ) {
        // ... notify ...
        a_success_callback();
    }
}

/**
 * @brief Publish a message using a REDIS channel.
 *
 * @param a_id
 * @param a_channel
 * @param a_object
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Publish (const Json::Value& a_object,
                                         const std::function<void()> a_success_callback,
                                         const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    osal::ConditionVariable cv;
    ev::Exception*          exception = nullptr;

    const std::string redis_channel = rcid_;
    const std::string redis_key     = rjid_;
    const std::string redis_message = json_writer_.write(a_object);

    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("PUBLISH", "%s", redis_message.c_str());
    
    ExecuteOnMainThread([this, &cv, &redis_channel, &redis_key, &redis_message, &exception] {
        // ... srtart by publishing a message ....
        ev::scheduler::Task* t = NewTask([this, &redis_channel, redis_message] () -> ::ev::Object* {

            return new ev::redis::Request(loggable_data_,
                                          "PUBLISH", { redis_channel, redis_message }
            );

        });
        // ... and if NOT transient ...
        if ( false == transient_ ) {
            // ... ser a permantent key ...
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
            // ... set expiration ...
            if ( -1 != validity_ ) {
                // ... if acceptable ...
                if ( validity_ > 0 ) {
                    // ... set expiration ...
                    t->Then([this, redis_key] (::ev::Object* a_object) -> ::ev::Object* {
                        // ... a string 'OK' is expected ...
                        ev::redis::Reply::EnsureIntegerReply(a_object);
                        // ... set expiration date ...
                        return new ::ev::redis::Request(loggable_data_,
                                                        "EXPIRE", { redis_key, std::to_string(validity_) }
                        );
                    })->Finally([&cv] (::ev::Object* a_object) {
                        //
                        // EXPIRE:
                        //
                        // Integer reply, specifically:
                        // - 1 if the timeout was set.
                        // - 0 if key does not exist or the timeout could not be set.
                        //
                        ev::redis::Reply::EnsureIntegerReply(a_object, 1);
                        // ... WAKE from REDIS operations ...
                        cv.Wake();
                    });
                } else {
                    // ... no expiration is required ....
                    t->Finally([&cv] (::ev::Object* a_object) {
                        // ... a string 'OK' is expected ...
                        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");
                        // ... WAKE from REDIS operations ...
                        cv.Wake();
                    });
                }
            } else {
                // ... no expiration set, validate 'SET' response ...
                t->Finally([&cv] (::ev::Object* a_object) {
                    // ... an integer reply is expected ...
                    ev::redis::Reply::EnsureIntegerReply(a_object);
                    // ... WAKE from REDIS operations ...
                    cv.Wake();
                });
            }
        } else {
            // ... transient, validate 'PUBLISH' response ...
            t->Finally([&cv] (::ev::Object* a_object) {
                // ... an integer reply is expected ...
                ev::redis::Reply::EnsureIntegerReply(a_object);
                // ... WAKE from REDIS operations ...
                cv.Wake();
            });
        }
        // ... catch all exceptions ....
        t->Catch([&cv, &exception] (const ::ev::Exception& a_ev_exception) {
            // ... copy it ...
            exception = new ev::Exception(a_ev_exception);
            // ... WAKE from REDIS operations ...
            cv.Wake();
        });
    }, /* a_blocking */ false);

    // ... WAIT until REDIS operations are completed ...
    cv.Wait();
    
    // ... an exception ocurred?
    if ( nullptr != exception ) {
        // ... notify?
        if ( nullptr != a_failure_callback ) {
            // ... copy ...
            const ::ev::Exception copy = ::ev::Exception(*exception);
            // ... release it ...
            delete exception;
            // ... notify ...
            a_failure_callback(copy);
        } else {
            // ... log it ...
            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "PUBLISH FAILED: %s",
                                            exception->what()
            );
            // ... release it ...
            delete exception;
        }
    } else if ( nullptr != a_success_callback ) {
        // ... notify ...
        a_success_callback();
    }
}


/**
 * @brief Get a job cancellation flag.
 */
bool ev::loop::beanstalkd::Job::ShouldCancel ()
{
    // ... first check cancellation flag ...
    const std::string redis_key = rjid_;
    
    osal::ConditionVariable cancellation_cv;
    
    ExecuteOnMainThread([this, &cancellation_cv, redis_key] {
        
        NewTask([this, redis_key] () -> ::ev::Object* {
            
            return new ev::redis::Request(loggable_data_,
                                          "HMGET",
                                          { /* key   */ redis_key, /* field */ "cancelled", /* field */ "status" }
            );
            
        })->Finally([this, &cancellation_cv] (::ev::Object* a_object) {
            
            //
            // HMGET:
            //
            // - An array of String valueS is expected:
            //
            const ::ev::redis::Value& value = ev::redis::Reply::EnsureArrayReply(a_object, /* a_size */ 2);
            cancelled_ = ( 0 == strcasecmp(value[0].String().c_str(), "true") );
            if ( value[1].String().length() > 0 ) {
                already_ran_ = ( 0 != strcasecmp(value[1].AsJSONObject().get("status", "").asCString(), "queued") );
            } else {
                already_ran_ = false;
            }
            
            cancellation_cv.Wake();
            
        })->Catch([this, &cancellation_cv] (const ::ev::Exception& a_ev_exception) {

            // ... log error ...
            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "HGET failed: %s",
                                            a_ev_exception.what()
            );
            // ... unlock ...
            cancellation_cv.Wake();
            
        });
        
    }, /* a_blocking */ false);
    
    cancellation_cv.Wait();
    
    // ... done ...
    return cancelled_;
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
 * @brief Schedule a callback on 'looper' thread.
 *
 * @param a_id
 * @param a_callback
 * @param a_deferred
 * @param a_recurrent
 */
void ev::loop::beanstalkd::Job::ScheduleCallbackOnLooperThread (const std::string& a_id, Job::LooperThreadCallback a_callback,
                                                                const size_t a_deferred, const bool a_recurrent)
{
    callbacks_ptr_->schedule_callback_on_the_looper_thread_(a_id, a_callback, a_deferred, a_recurrent);
}

/**
 * @brief Try to cancel a previously scheduled callback on 'looper' thread.
 *
 * @param a_id
 */
void ev::loop::beanstalkd::Job::TryCancelCallbackOnLooperThread (const std::string& a_id)
{
    callbacks_ptr_->try_cancel_callback_on_the_looper_thread_(a_id);
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
 * @brief Push a 'beanstalkd' job.
 *
 * @param a_tube
 * @param a_payload
 * @param a_ttr
 */
void ev::loop::beanstalkd::Job::PushJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)
{
    callbacks_ptr_->on_push_job_(a_tube, a_payload, a_ttr);
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
                      [&o_code, &o_data, &o_elapsed, &cv] (const char* /* a_uri */, const char* a_json, const char* /* a_error */, uint16_t a_status, uint64_t a_elapsed) {
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
 * @brief Perform an HTTP GET request.
 *
 * param a_url
 * param o_code
 * param o_data
 * param o_elapsed
 * param o_url
 */
void ev::loop::beanstalkd::Job::HTTPGet (const Json::Value& a_url,
                                         uint16_t& o_code, std::string& o_data, uint64_t& o_elapsed, std::string& o_url)
{
    o_url =  a_url.asString();

    const auto dlsp = std::chrono::steady_clock::now();
    
    HTTPGet(/* a_url */o_url,
            /* a_headers */ {},
            /* a_success_callback */
            [&o_code, &o_data] (const ::ev::curl::Value& a_value) {
                o_code = a_value.code();
                o_data = a_value.body();
            },
            /* a_failure_callback */
            [&o_code, &o_data] (const ::ev::Exception& a_ev_exception) {
                o_code = 500;
                o_data = a_ev_exception.what();
            }
    );
    
    o_elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - dlsp).count());
}

/**
 * @brief Perform an HTTP GET request.
 *
 * param a_url
 * param a_headers
 * param a_success_callback
 * param a_failure_callback
 */
void ev::loop::beanstalkd::Job::HTTPGet (const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers,
                                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    HTTPSyncExec([this, &a_url, &a_headers] (EV_CURL_HTTP_SUCCESS_CALLBACK a_a, EV_CURL_HTTP_FAILURE_CALLBACK a_b) {
        http_.GET(
                  loggable_data_,
                  /* a_url */
                  a_url,
                  /* a_headers */
                  &a_headers,
                  /* a_success_callback */
                  a_a,
                  /* a_failure_callback */
                  a_b
        );
    }, a_success_callback, a_failure_callback);
}


/**
 * @brief Perform an HTTP request and write the file to the temporary directory.
 *
 * param a_url
 * param a_headers
 * param a_validity
 * param a_prefix
 * param a_extension
 * param a_success_callback
 * param a_failure_callback
 */
void ev::loop::beanstalkd::Job::HTTPGetFile (const std::string& a_url, const EV_CURL_HEADERS_MAP& a_headers,
                                             const uint64_t& a_validity, const std::string& a_prefix, const std::string& a_extension,
                                             EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    std::string local_uri;
    if ( osal::File::Status::EStatusOk != osal::File::UniqueFileName(EnsureOutputDir(a_validity), ( 0 != a_prefix.length() ? a_prefix : tube_ ), a_extension, local_uri) ) {
        throw ::ev::Exception("%s",  "Unable to create an unique file name for the HTTP request response!");
    }

    HTTPSyncExec([this, &a_url, &a_headers, &local_uri] (EV_CURL_HTTP_SUCCESS_CALLBACK a_a, EV_CURL_HTTP_FAILURE_CALLBACK a_b) {
        
        http_.GET(
                  loggable_data_,
                  /* a_url */
                  a_url,
                  /* a_headers */
                  &a_headers,
                  /* a_success_callback */
                  a_a,
                  /* a_failure_callback */
                  a_b,
                  /* o_uri */
                  local_uri
        );
        
    }, a_success_callback, a_failure_callback);
}

/**
 * @brief Perform an HTTP request and write the file to the temporary directory.
 *
 * param a_uri
 * param a_url
 * param a_headers
 * param a_success_callback
 * param a_failure_callback
 */
void ev::loop::beanstalkd::Job::HTTPPostFile (const std::string& a_uri, const std::string& a_url,
                                              const EV_CURL_HEADERS_MAP& a_headers,
                                              EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{    
    HTTPSyncExec([this, &a_uri, &a_url, &a_headers] (EV_CURL_HTTP_SUCCESS_CALLBACK a_a, EV_CURL_HTTP_FAILURE_CALLBACK a_b) {
        
        http_.POST(loggable_data_,
                   /* a_uri */
                   a_uri,
                   /* a_url */
                   a_url,
                   /* a_headers */
                   &a_headers,
                   /* a_success_callback */
                   a_a,
                   /* a_failure_callback */
                   a_b
        );
        
    }, a_success_callback, a_failure_callback);
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
void ev::loop::beanstalkd::Job::ExecuteQuery (const std::string& a_query, Json::Value& o_result, const bool a_dont_auto_parse = false)
{
    o_result = Json::Value(Json::ValueType::objectValue);
    o_result["status_code"] = 500;

    osal::ConditionVariable cv;

    ExecuteOnMainThread([this, &a_query, &o_result, &cv, &a_dont_auto_parse] () {

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

        })->Finally([this, a_query, &o_result, &cv, &a_dont_auto_parse] (::ev::Object* a_object) {

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

            o_result["table"] = Json::Value(Json::ValueType::arrayValue);
            
            if (a_dont_auto_parse) {
                const int rows_count    = value.rows_count();
                const int columns_count = value.columns_count();
                Json::Reader reader;

                for ( int row_idx = 0 ; row_idx < rows_count ; ++row_idx ) {
                    Json::Value& line = o_result["table"].append(Json::Value(Json::ValueType::objectValue));
                    for ( int column_idx = 0 ; column_idx < columns_count ; ++column_idx ) {
                        line[value.column_name(column_idx)] = value.raw_value(row_idx, column_idx);
                    }
                }
            } else {
                ToJSON(value, o_result["table"]);
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
const Json::Value& ev::loop::beanstalkd::Job::GetJSONObject (const Json::Value& a_parent, const char* const a_key, const Json::ValueType& a_type, const Json::Value* a_default,
                                                             const char* const a_error_prefix_msg) const
{
    // ... try to obtain a valid JSON object ..
    try {
        const Json::Value& value = a_parent[a_key];
        if ( true == value.isNull() ) {
            if ( nullptr != a_default ) {
                return *a_default;
            } else if ( Json::ValueType::nullValue == a_type ) {
                return Json::Value::null;
            } /* else { } */
        } else if ( value.type() == a_type || true == value.isConvertibleTo(a_type) ) {
            return value;
        }
        // ... if it reached here, requested object is not set or has an invalid type ...
        throw ::ev::Exception("%sJSON value for key '%s' - type mismatch: got %s, expected %s!",
                              a_error_prefix_msg,
                              a_key, JSONValueTypeAsCString(value.type()), JSONValueTypeAsCString(a_type)
        );
    } catch (const Json::Exception& a_json_exception ) {
           throw ::ev::Exception("%s", a_json_exception.what());
    }
}

/**
 * @brief Merge JSON Value.
 *
 * @param a_lhs Primary value.
 * @param a_rhs Override value.
 */
void ev::loop::beanstalkd::Job::MergeJSONValue (Json::Value& a_lhs, const Json::Value& a_rhs) const
{
    if ( false == a_lhs.isObject() || false == a_rhs.isObject() ) {
        return;
    }
    for ( const auto& k : a_rhs.getMemberNames() ) {
        if ( true == a_lhs[k].isObject() && true == a_rhs[k].isObject() ) {
            MergeJSONValue(a_lhs[k], a_rhs[k]);
        } else {
            a_lhs[k] = a_rhs[k];
        }
    }
}

/**
 * @brief Serialize a JSON string to a JSON Object.
 *
 * @param a_value JSON string to parse.
 * @param o_value JSON object to fill.
 */
void ev::loop::beanstalkd::Job::ParseJSON (const std::string& a_value, Json::Value& o_value) const
{
    try {
        Json::Reader reader;
        if ( false == reader.parse(a_value, o_value) ) {
            const auto errors = reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw ::ev::Exception("An error ocurred while parsing '%s as JSON': %s!",
                                      a_value.c_str(), reader.getFormatedErrorMessages().c_str()
                );
            } else {
                throw ::ev::Exception("An error ocurred while parsing '%s' as JSON!",
                                      a_value.c_str()
                );
            }
        }
    } catch (const Json::Exception& a_json_exception ) {
        throw ::ev::Exception("%s", a_json_exception.what());
    }
}

/**
 * @brief Serialize an PostgreSQL value to a JSON Object.
 *
 * @param a_valu  PostgreSQL value.
 * @param o_value JSON object to fill.
 */
void ev::loop::beanstalkd::Job::ToJSON (const ev::postgresql::Value& a_value, Json::Value& o_value) const
{
    if ( false == o_value.isArray() ) {
        throw ::ev::Exception("Unexpected object type: got " UINT8_FMT " expected an array", static_cast<uint8_t>(a_value.type_));
    }
    try {
        Json::Reader reader;
        for ( int row = 0 ; row < a_value.rows_count() ; ++row ) {
            Json::Value& record = o_value.append(Json::Value(Json::ValueType::objectValue));
            for ( int column = 0 ; column < a_value.columns_count() ; ++column ) {
                const char* const raw_value = a_value.raw_value(/* a_row */ row, /* a_column */ column);
                if ( nullptr == raw_value || 0 == strlen(raw_value) ) {
                    record[a_value.column_name(column)] = Json::Value::null;
                } else if ( JSONBOID == a_value.column_type(column) ) {
                    ParseJSON(raw_value, record[a_value.column_name(column)]);
                } else if ( DATEOID == a_value.column_type(column) || TIMEOID == a_value.column_type(column) || TIMESTAMPOID == a_value.column_type(column) ) {
                    record[a_value.column_name(column)] = raw_value;
                } else if ( false == reader.parse(raw_value, record[a_value.column_name(column)]) ) { // ... using reader to try to translate values ...
                    // ... on failure, keep as it is ...
                    record[a_value.column_name(column)] = raw_value;
                }
            }
        }
    } catch (const Json::Exception& a_json_exception) {
        throw ev::Exception("%s", a_json_exception.what());
    }
}

/**
 * @brief Serialize an cURL value to a JSON Object.
 *
 * @param a_valu  cURL value.
 * @param o_value JSON object to fill.
 */
void ev::loop::beanstalkd::Job::ToJSON (const ev::curl::Value& a_value, Json::Value& o_value) const
{
    const auto it = a_value.headers().find("content-type");
    // ... ensure Content-Type header is set ...
    if ( a_value.headers().end() == it || 0 != strncasecmp("application/json", it->second[0].c_str(), sizeof(char)* 16) ) {
        throw ::ev::Exception("%s", "Invalid or missing 'Content-Type' header!");
    }
    // ... ensure body has valid json ...
    ParseJSON(a_value.body(), o_value);
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
* @param a_id      REDIS channel id.
* @param a_message Channel's message.
*/
EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK ev::loop::beanstalkd::Job::OnSignalsChannelMessageReceived (const std::string& a_name, const std::string& a_message)
{
    ExecuteOnMainThread([this, a_name, a_message] () {

        Json::Value  object;
        Json::Reader reader;
        try {
            // ... if message is a valid JSON ...
            if ( true == reader.parse(a_message, object, false) ) {
                // ... and it has the 'id' and 'status' field set ...
                if ( true ==  object.isMember("id") && true == object.isMember("status") ) {
                    // ...grab fields ...
                    const Json::Value& id     = object.get("id", -1);
                    const Json::Value& status = object.get("status", Json::Value::null);
                    // ... if it's for the current job ...
                    if ( 0 == rjnr_.compare(std::to_string(static_cast<int64_t>(id.asInt64()))) ) {
                        // ... and it was cancelled ...
                        if ( true == status.isString() && 0 == strcasecmp(status.asCString(), "cancelled") ) {
                            // ... log ...
                            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("CANCELLED", "%s: %s",
                                                            a_name.c_str(), a_message.c_str()
                            );
                            // ... and mark as cancelled ...
                            cancelled_ = true;
                        }
                    }
                    // ... notify listener?
                    if ( nullptr != signals_channel_listener_ ) {
                        signals_channel_listener_(id.asUInt64(), status.asString(), object);
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
#pragma mark [Private] - REDIS Callback
#endif

/**
 * @brief Called when a REDIS connection has been lost.
 */
void ev::loop::beanstalkd::Job::OnREDISConnectionLost ()
{
    OnFatalException(ev::Exception("REDIS connection lost:\n unable to reconnect to REDIS!"));
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Execute an HTTP request synchronously on 'main' thread..
 *
 * @param a_block Execution block
 * @param a_success_callback Function to call on success to deliver result.
 * @param a_failure_callback Function to call on failure due to an exception.
 */
void ev::loop::beanstalkd::Job::HTTPSyncExec (std::function<void(EV_CURL_HTTP_SUCCESS_CALLBACK, EV_CURL_HTTP_FAILURE_CALLBACK)> a_block,
                                              EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    osal::ConditionVariable cv;
    
    ::ev::curl::Value* value     = nullptr;
    ::ev::Exception*   exception = nullptr;

    ExecuteOnMainThread([&a_block, &value, &exception, &cv] {
        
        a_block(/* a_success_callback */
                [&value, &cv](const ::ev::curl::Value& a_value) {
                  // ... copy value ...
                  value = new ::ev::curl::Value(a_value);
                  // ... WAKE from REDIS operations ...
                  cv.Wake();
                },
                /* a_failure_callback */
                [&exception, &cv](const ::ev::Exception& a_ev_exception) {
                  // ... copy exception ...
                  exception = new ::ev::Exception(a_ev_exception);
                  // ... WAKE from REDIS operations ...
                  cv.Wake();
                }
        );
    }, /* a_blocking */ false);
    
    // ... WAIT until REDIS operations are completed ...
    cv.Wait();

    // ... an exception ocurred?
    if ( nullptr != exception ) {
        // ... release value ...
        if ( nullptr != value ) {
            delete value;
        }
        // ... notify?
        if ( nullptr != a_failure_callback ) {
            // ... copy ...
            const ::ev::Exception copy = ::ev::Exception(*exception);
            // ... release it ...
            delete exception;
            // ... notify ...
            a_failure_callback(copy);
        } else {
            // ... log it ...
            EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("ERROR", "HTTP GET FILE FAILED: %s",
                                            exception->what()
            );
            // ... release it ...
            delete exception;
        }
    } else {
        // ... value must exist!
        CC_ASSERT(nullptr != value);
        // ... notify?
        if ( nullptr != a_success_callback ) {
            try {
                a_success_callback(*value);
            } catch (...) {
                ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
            }
        }
        // ... release value ...
        delete value;
    }
}
