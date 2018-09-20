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

/**
 * @brief Default constructor.
 *
 * @param a_config
 */
ev::loop::beanstalkd::Job::Job (const Config& a_config)
    : config_(a_config),
    redis_default_validity_(3600),
    redis_signal_channel_(config_.service_id_ + ":job-signal"),
    redis_key_prefix_(config_.service_id_ + ":jobs:" + config_.tube_ + ':'),
    json_api_(config_.loggable_data_ref_)
{
    id_       = 0;
    validity_ = redis_default_validity_;
    
    ev::scheduler::Scheduler::GetInstance().Register(this);
    
    osal::ConditionVariable cv;
    
    ev::scheduler::Scheduler::GetInstance().CallOnMainThread(this, [this, &cv] () {
        
        
        const std::string channel = ( config_.service_id_ + ":job-signal" );
        
        ::ev::redis::subscriptions::Manager::GetInstance().SubscribeChannels({ channel },
                                                                             /* a_status_callback */
                                                                             [this, &cv](const std::string& a_name_or_pattern,
                                                                                         const ::ev::redis::subscriptions::Manager::Status& a_status) -> EV_REDIS_SUBSCRIPTIONS_DATA_POST_NOTIFY_CALLBACK {
                                                                                 if ( ::ev::redis::subscriptions::Manager::Status::Subscribed == a_status ) {
                                                                                     cv.Wake();
                                                                                 }
                                                                                 return nullptr;
                                                                             },
                                                                             /* a_data_callback */
                                                                             std::bind(&ev::loop::beanstalkd::Job::JobSignalsDataCallback, this,
                                                                                       std::placeholders::_1, std::placeholders::_2
                                                                             ),
                                                                             /* a_client */
                                                                             this
        );
        
    });
    
    cv.Wait();
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Job::~Job ()
{
    ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Prepare a job common configuration.
 *
 * @param a_id                  Job ID
 * @param a_payload             Job Payload
 * @param a_callback            Success / Failure callback.
 * @param a_cancelled_callback
 */
void ev::loop::beanstalkd::Job::Consume (const int64_t& a_id, const Json::Value& a_payload,
                                         const ev::loop::beanstalkd::Job::SuccessCallback& a_success_callback,
                                         const ev::loop::beanstalkd::Job::CancelledCallback& a_cancelled_callback)
{
    //
    // JOB Configuration
    //
    const Json::Value redis_channel = a_payload.get("id", Json::nullValue); // id: a.k.a. redis <beanstalkd_tube_name>:<redis_job_id>
    if ( true == redis_channel.isNull() || false == redis_channel.isString() ||  0 == redis_channel.asString().length() ) {
        throw ev::Exception("Missing or invalid 'id' object!");
    }
    id_       = a_id; // beanstalkd number id
    channel_  = redis_channel.asString();
    validity_ = a_payload.get("validity", redis_default_validity_).asInt64();
    response_ = Json::Value::null;
    
    //
    // JSONAPI Configuration - optional
    //
    if ( true == a_payload.isMember("jsonapi") ) {
        ConfigJSONAPI(a_payload["jsonapi"]);
    }
    
    //
    // Configure Log
    //
    // TODO config_.loggable_data_ref_.SetTag(redis_key_prefix_ + channel_);
    
    //
    // Run JOB
    //
    Run(a_id, a_payload, a_success_callback, a_cancelled_callback);
}

#ifdef __APPLE__
#pragma make -
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
    Publish(a_payload, validity_);
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
    
    ev::scheduler::Scheduler::GetInstance().CallOnMainThread(this, [this, &a_channel, &a_success_callback, &a_failure_callback, &cv, &redis_message] {
        
        NewTask([this, &a_channel, &redis_message] () -> ::ev::Object* {
            
            return new ev::redis::Request(config_.loggable_data_ref_,
                                          "PUBLISH", { a_channel, redis_message }
            );
            
        })->Finally([this, &cv, a_success_callback] (::ev::Object* a_object) {
            
            // ... an integer reply is expected ...
            ev::redis::Reply::EnsureIntegerReply(a_object);
            
            // ... notify ...
            if ( nullptr != a_success_callback ) {
                a_success_callback();
            }
            
            cv.Wake();
            
        })->Catch([this, &cv, a_failure_callback] (const ::ev::Exception& a_ev_exception) {
            
            // ... notify ...
            if ( nullptr != a_failure_callback ) {
                a_failure_callback(a_ev_exception);
            }
            
            cv.Wake();
            
        });
        
    });
    
    cv.Wait();
}

/**
 * @brief Publish a message using a REDIS channel.
 *
 * @param a_object
 * @param a_validity
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::loop::beanstalkd::Job::Publish (const Json::Value& a_object, const int64_t a_validity,
                                         const std::function<void()> a_success_callback,
                                         const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback)
{
    osal::ConditionVariable cv;
    
    const std::string redis_key     = redis_key_prefix_ + channel_;
    const std::string redis_message = json_writer_.write(a_object);
    
    // TODO
//    NRS_CASPER_PRINT_QUEUE_PRINTER_LOG("queue",
//                                       "Publishing to REDIS channel '%s' : %s",
//                                       redis_key.c_str(),
//                                       redis_message.c_str()
//    );
    
    ev::scheduler::Scheduler::GetInstance().CallOnMainThread(this, [this, a_validity, a_success_callback, a_failure_callback, &cv, &redis_key, &redis_message] {
        
        ev::scheduler::Task* t = NewTask([this, &redis_key, redis_message] () -> ::ev::Object* {
            
            return new ev::redis::Request(config_.loggable_data_ref_,
                                          "PUBLISH", { redis_key, redis_message }
            );
            
        })->Then([this, redis_key, redis_message] (::ev::Object* a_object) -> ::ev::Object* {
            
            // ... an integer reply is expected ...
            ev::redis::Reply::EnsureIntegerReply(a_object);
            
            // ... make it permanent ...
            return new ev::redis::Request(config_.loggable_data_ref_,
                                          "HSET",
                                          {
                                              /* key   */ redis_key,
                                              /* field */ "status", redis_message
                                          }
            );
            
        });
        
        if ( -1 != a_validity ) {
            
            if ( a_validity > 0 ) {
                
                t->Then([this, redis_key, a_validity] (::ev::Object* a_object) -> ::ev::Object* {
                    
                    // ... a string 'OK' is expected ...
                    ev::redis::Reply::EnsureIntegerReply(a_object);
                    
                    // ... set expiration date ...
                    return new ::ev::redis::Request(config_.loggable_data_ref_,
                                                    "EXPIRE", { redis_key, std::to_string(a_validity) }
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
                
                t->Finally([this, &cv, a_success_callback] (::ev::Object* a_object) {
                    
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
            // ...just a publish ...
            t->Finally([this, &cv, a_success_callback] (::ev::Object* a_object) {
                
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
            // TODO
//            NRS_CASPER_PRINT_QUEUE_PRINTER_LOG("error",
//                                               "PUBLISH failed: %s",
//                                               a_ev_exception.what()
//                                               );
            
            // ... notify ...
            if ( nullptr != a_failure_callback ) {
                a_failure_callback(a_ev_exception);
            }
            
            cv.Wake();
            
        });
        
    });
    
    cv.Wait();
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
 */
void ev::loop::beanstalkd::Job::ExecuteQuery (const std::string& a_query, Json::Value& o_result)
{
    o_result = Json::Value(Json::ValueType::objectValue);
    o_result["status_code"] = 500;
    
    osal::ConditionVariable cv;
    
    ev::scheduler::Scheduler::GetInstance().CallOnMainThread(this, [this, &a_query, &o_result, &cv] () {
        
        NewTask([this, a_query] () -> ::ev::Object* {
            
            return new ::ev::postgresql::Request(config_.loggable_data_ref_, a_query);
            
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
            
        })->Finally([this, a_query, &o_result, &cv] (::ev::Object* a_object) {
            
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
                    line[std::to_string(column_idx)] = value.raw_value(row_idx, column_idx);
                }
            }
            
            o_result["status_code"] = 200;
            
            cv.Wake();
            
        })->Catch([a_query, &o_result, &cv] (const ::ev::Exception& a_ev_exception) {
            
            o_result["status_code"] = 500;
            o_result["exception"]   = a_ev_exception.what();
            
            cv.Wake();
            
        });
        
    });
    
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
#pragma mark [Private] - Scheduler
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
    ev::scheduler::Scheduler::GetInstance().CallOnMainThread(this, [this, &a_name, &a_message] () {
        
        Json::Value  object;
        Json::Reader reader;
        try {
            if ( true == reader.parse(a_message, object, false) ) {
                if ( true ==  object.isMember("id") && true == object.isMember("status") ) {
                    const Json::Value id = object.get("id", -1);
                    // TODO
                    //                    if ( 0 == job_channel_.compare(std::to_string(static_cast<int64_t>(id.asInt64()))) ) {
                    //                        const Json::Value value = object.get("status", null_object_);
                    //                        if ( true == value.isString() && 0 == strcasecmp(value.asCString(), "cancelled") ) {
                    //                            NRS_CASPER_PRINT_QUEUE_PRINTER_LOG("queue",
                    //                                                               "Received from REDIS channel '%s': %s",
                    //                                                               a_name.c_str(),
                    //                                                               a_message.c_str()
                    //                                                               );
                    //                            job_cancelled_ = true;
                    //                        }
                    //                    }
                }
            }
        } catch (const Json::Exception& /* a_json_exception */) {
            // ... eat it ...
        }
        
    }
                                                             );
    
    return nullptr;
}

#ifdef __APPLE__
#pragma -
#endif

/**
 * @brief Called when a REDIS connection has been lost.
 */
void ev::loop::beanstalkd::Job::OnREDISConnectionLost ()
{
    config_.fatal_exception_callback_(ev::Exception("REDIS connection lost:\n unable to reconnect to REDIS!"));
    // TODO FIX AT CASPER-PRINT-QUEUE
}

