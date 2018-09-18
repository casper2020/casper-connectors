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
    : config_(a_config), json_api_(config_.loggable_data_ref_)
{
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


/**
 * @brief Publish a message using a REDIS channel.
 *
 * @param a_id
 * @param a_channel
 * @param a_object
 */
void ev::loop::beanstalkd::Job::Publish (const int64_t& a_id, const std::string& a_channel, const Json::Value& a_object,
                                         const std::function<void()> a_success_callback, const std::function<void(const ev::Exception& a_ev_exception)> a_failure_callback,
                                         const int64_t a_validity)
{
    
    const std::string redis_key     = config_.service_id_ + ":jobs:" + config_.tube_ + ':' + a_channel;
    const std::string redis_message = json_writer_.write(a_object);
    
//    NRS_CASPER_PRINT_QUEUE_PRINTER_LOG("queue",
//                                       "Publishing to REDIS channel '%s' : %s",
//                                       (config_.service_id_ + ':' + config_.tube_ + ':' + a_channel).c_str(),
//                                       redis_message.c_str()
//    );

    osal::ConditionVariable cv;

    ev::scheduler::Scheduler::GetInstance().CallOnMainThread(this, [this, &a_channel, &a_id, &a_validity, a_success_callback, a_failure_callback, &cv, &redis_key, &redis_message]() {
    
        ev::scheduler::Task* t = NewTask([this, a_channel, redis_message] () -> ::ev::Object* {
            
            return new ev::redis::Request(config_.loggable_data_ref_,
                                          "PUBLISH", { (config_.service_id_ + ':' + config_.tube_ + ':' + a_channel), redis_message }
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
        
        t->Catch([this, a_id, &cv, a_failure_callback] (const ::ev::Exception& a_ev_exception) {
            
            // ... log error ...
            //TODO...
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

