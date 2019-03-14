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
 * @param a_tube
 * @param a_config
 * @param a_loggable_data_ref
 */
ev::loop::beanstalkd::Job::Job (const std::string& a_tube, const Config& a_config, const ev::Loggable::Data& a_loggable_data)
    : tube_(a_tube), config_(a_config),
    redis_signal_channel_(config_.service_id_ + ":job-signal"),
    redis_key_prefix_(config_.service_id_ + ":jobs:" + tube_ + ':'),
    redis_channel_prefix_(config_.service_id_ + ':' + tube_ + ':'),
    loggable_data_(a_loggable_data),
    http_(loggable_data_),
    json_api_(loggable_data_, /* a_enable_task_cancellation */ false)
{
    id_                   = 0;

    progress_             = Json::Value(Json::ValueType::objectValue);
    progress_["status"]   = "in-progress";
    progress_["progress"] = 0.0;
    response_             = Json::Value::null;
    
    validity_             = 0;
    transient_            = config_.transient_;
    cancelled_            = false;
        
    chdir_hrt_.seconds_   = static_cast<uint8_t>(0);
    chdir_hrt_.minutes_   = static_cast<uint8_t>(0);
    chdir_hrt_.hours_     = static_cast<uint8_t>(0);
    chdir_hrt_.day_       = static_cast<uint8_t>(0);
    chdir_hrt_.month_     = static_cast<uint8_t>(0);
    chdir_hrt_.year_      = static_cast<uint16_t>(0);
    
    hrt_buffer_[0]        = '\0';
    
    ev::scheduler::Scheduler::GetInstance().Register(this);
   
    json_writer_.omitEndingLineFeed();
    
    callbacks_ptr_ = nullptr;
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
                                                                             [this] (const std::string& a_name, const std::string& a_message) -> EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK {
                                                                                 
                                                                                 ExecuteOnMainThread([this, a_name, a_message] {
                                                                                     
                                                                                     Json::Value  object;
                                                                                     Json::Reader reader;
                                                                                     try {
                                                                                         if ( true == reader.parse(a_message, object, false) ) {
                                                                                             if ( true ==  object.isMember("id") && true == object.isMember("status") ) {
                                                                                                 const Json::Value id = object.get("id", -1);
                                                                                                 if ( 0 == channel_.compare(std::to_string(static_cast<int64_t>(id.asInt64()))) ) {
                                                                                                     const Json::Value value = object.get("status", Json::Value::null);
                                                                                                     if ( true == value.isString() && 0 == strcasecmp(value.asCString(), "cancelled") ) {
                                                                                                         EV_LOOP_BEANSTALK_JOB_LOG("queue",
                                                                                                                                   "Received from REDIS channel '%s': %s",
                                                                                                                                   a_name.c_str(),
                                                                                                                                   a_message.c_str()
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
                                                                             },
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
    // JOB Configuration
    //
    const Json::Value redis_channel = a_payload.get("id", Json::nullValue); // id: a.k.a. redis <beanstalkd_tube_name>:<redis_job_id>
    if ( true == redis_channel.isNull() || false == redis_channel.isString() ||  0 == redis_channel.asString().length() ) {
        throw ev::Exception("Missing or invalid 'id' object!");
    }
    id_       = a_id; // beanstalkd number id
    channel_   = redis_channel.asString();
    validity_  = a_payload.get("validity", 3600).asInt64();
    transient_ = a_payload.get("transient", config_.transient_).asBool();
    cancelled_ = false;
    response_  = Json::Value::null;

    //
    // JSONAPI Configuration - optional
    //
    if ( true == a_payload.isMember("jsonapi") ) {
        ConfigJSONAPI(a_payload["jsonapi"]);
    }

    //
    // Configure Log
    //
    loggable_data_.SetTag(redis_key_prefix_ + channel_);
    
    const auto cancelled_callback = [this, &a_cancelled_callback] () {
        
        Json::Value i18n_array = Json::Value(Json::ValueType::arrayValue);
        i18n_array.append("i18n_job_cancelled");
        i18n_array.append(Json::Value(Json::ValueType::objectValue));
        
        Json::Value progress_object = Json::Value(Json::ValueType::objectValue);
        progress_object["message"] = i18n_array;
        progress_object["status"] = "cancelled";

        PublishProgress(progress_object);
        
        a_cancelled_callback();
        
    };
    
    GetJobCancellationFlag();
    
    if ( true == IsCancelled() ) {
        cancelled_callback();
    } else {
        Run(a_id, a_payload, a_completed_callback, cancelled_callback);
    }

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
 * @brief Publish a 'job cancelled' message to 'job-signals' channel.
 */
void ev::loop::beanstalkd::Job::PublishCancelled ()
{
    response_            = Json::Value(Json::ValueType::objectValue);
    response_["id"]      = id_;
    response_["status"]  = "cancelled";
    response_["channel"] = channel_;

    Publish(redis_signal_channel_, response_);
}

/**
 * @brief Publish a 'job finished' message to 'job-signals' channel.
 *
 * @param a_payload
 */
void ev::loop::beanstalkd::Job::PublishFinished (const Json::Value& a_payload)
{
    PublishProgress(a_payload);

    response_            = Json::Value(Json::ValueType::objectValue);
    response_["id"]      = id_;
    response_["status"]  = "finished";
    response_["channel"] = channel_;

    Publish(redis_signal_channel_, response_);
}

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
 * @brief Publish a 'job progress' message.
 *
 * @param a_message
 */
void ev::loop::beanstalkd::Job::PublishProgress (const ev::loop::beanstalkd::Job::Progress& a_message)
{
    Json::Value i18n_array = Json::Value(Json::ValueType::arrayValue);

    i18n_array.append(a_message.key_);
    if ( nullptr != a_message.args_ ) {
        i18n_array.append(*a_message.args_);
    }

    progress_["message"]  = i18n_array;
    progress_["progress"] = a_message.value_;

    Publish(progress_);
}

/**
 * @brief Publish signal.
 *
 * @param a_object
 */
void ev::loop::beanstalkd::Job::PublishSignal (const Json::Value& a_object)
{
    Publish(redis_signal_channel_, a_object);
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

    EV_LOOP_BEANSTALK_JOB_LOG("queue",
                              "Publishing to REDIS channel '%s' : %s",
                              redis_key.c_str(),
                              redis_message.c_str()
    );

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

            // ... log error ...
            EV_LOOP_BEANSTALK_JOB_LOG("error",
                                      "PUBLISH failed: %s",
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
void ev::loop::beanstalkd::Job::GetJobCancellationFlag ()
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
        
        http_.GET(/* a_url */
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
    cc::UTCTime::HumanReadable now_hrt = cc::UTCTime::ToHumanReadable(cc::UTCTime::OffsetBy(a_validity));
    
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
        
        EV_LOOP_BEANSTALK_JOB_LOG("queue",
                                  "Changing output dir to %s...",
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
                            EV_LOOP_BEANSTALK_JOB_LOG("queue",
                                                      "Received from REDIS channel '%s': %s",
                                                      a_name.c_str(),
                                                      a_message.c_str()
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
