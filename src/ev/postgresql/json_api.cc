/**
 * @file jsonapi.cc - PostgreSQL
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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

#include "ev/postgresql/json_api.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/reply.h"
#include "ev/postgresql/error.h"

#include <algorithm>

#include <sstream>

#include <functional> // std::bind

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref
 * @param a_enable_task_cancellation
 */
::ev::postgresql::JSONAPI::JSONAPI (const ::ev::Loggable::Data& a_loggable_data_ref, bool a_enable_task_cancellation)
    : loggable_data_ref_(a_loggable_data_ref), enable_task_cancellation_(a_enable_task_cancellation)
{
    if ( true == enable_task_cancellation_ ) {
        uris_.invalidate_ = std::bind(&::ev::postgresql::JSONAPI::InvalidateHandler, this);
    }
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data_ref
 */
::ev::postgresql::JSONAPI::JSONAPI (const ::ev::postgresql::JSONAPI& a_json_api)
    : loggable_data_ref_(a_json_api.loggable_data_ref_), enable_task_cancellation_(a_json_api.enable_task_cancellation_)
{
    uris_.SetBase(a_json_api.uris_.GetBase());
    user_id_          = a_json_api.user_id_;
    entity_id_        = a_json_api.entity_id_;
    entity_schema_    = a_json_api.entity_schema_;
    sharded_schema_   = a_json_api.sharded_schema_;
    subentity_schema_ = a_json_api.subentity_schema_;
    subentity_prefix_ = a_json_api.subentity_prefix_;
    if ( true == enable_task_cancellation_ ) {
        uris_.invalidate_ = std::bind(&::ev::postgresql::JSONAPI::InvalidateHandler, this);
    }
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor
 */
::ev::postgresql::JSONAPI::~JSONAPI ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

#ifdef __APPLE__
#pragma mark -  API Method(s) / Function(s) - implementation
#endif

/**
 * @brief An API to GET data using a subrequest that will be processed by nginx_postgres submodule.
 *
 * @param a_uri
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Get (const std::string& a_uri, ::ev::postgresql::JSONAPI::Callback a_callback,
                                     std::string* o_query)
{
    
    Get(loggable_data_ref_, a_uri, a_callback, o_query);
}

/**
 * @brief An API to GET data using a subrequest that will be processed by nginx_postgres submodule.
 *
 * @param a_loggable_data
 * @param a_uri
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Get (const ::ev::Loggable::Data& a_loggable_data,
                                     const std::string& a_uri, ::ev::postgresql::JSONAPI::Callback a_callback,
                                     std::string* o_query)
{
    /*
     * Build Query
     */
    std::stringstream ss;
    ss << "SELECT response,http_status FROM jsonapi('GET', '" << a_uri << "', '', '" << user_id_ << "', '" << entity_id_ << "', '" << entity_schema_ << "', '" << sharded_schema_ << "', '" << subentity_schema_ << "', '" << subentity_prefix_ << "');";
    
    /*
     * Run query ( asynchronously )...
     */
    AsyncQuery(a_loggable_data, ss.str(), a_callback, o_query);
}

/**
 * @brief An API to POST data using a subrequest that will be processed CASPER CONNECTORS POSTGRES module.
 *
 * @param a_uri
 * @param a_body
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Post (const std::string& a_uri, const std::string& a_body, ::ev::postgresql::JSONAPI::Callback a_callback,
                                      std::string* o_query)
{
    Post(loggable_data_ref_, a_uri, a_body, a_callback, o_query);
}

/**
 * @brief An API to POST data using a subrequest that will be processed CASPER CONNECTORS POSTGRES module.
 *
 * @param a_loggable_data
 * @param a_uri
 * @param a_body
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Post (const ::ev::Loggable::Data& a_loggable_data,
                                      const std::string& a_uri, const std::string& a_body, ::ev::postgresql::JSONAPI::Callback a_callback,
                                      std::string* o_query)
{
    /*
     * Build Query
     */
    std::string body;
    SQLEscape(a_body, body);

    std::stringstream ss;
    ss << "SELECT response,http_status FROM jsonapi('POST','" << a_uri << "','" << body <<  "','" << user_id_ << "', '" << entity_id_ << "', '" << entity_schema_ << "', '" << sharded_schema_ << "', '" << subentity_schema_ << "', '" << subentity_prefix_ << "');";
    
    /*
     * Run query ( asynchronously )...
     */
    AsyncQuery(a_loggable_data, ss.str(), a_callback, o_query);
}

/**
 * @brief An API to PATCH data using a subrequest that will be processed by CASPER CONNECTORS POSTGRES module.
 *
 * @param a_uri
 * @param a_body
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Patch (const std::string& a_uri, const std::string& a_body, ::ev::postgresql::JSONAPI::Callback a_callback,
                                       std::string* o_query)
{
    Patch(loggable_data_ref_, a_uri, a_body, a_callback, o_query);
}

/**
 * @brief An API to PATCH data using a subrequest that will be processed by CASPER CONNECTORS POSTGRES module.
 *
 * @param a_loggable_data
 * @param a_uri
 * @param a_body
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Patch (const ::ev::Loggable::Data& a_loggable_data,
                                       const std::string& a_uri, const std::string& a_body, ::ev::postgresql::JSONAPI::Callback a_callback,
                                       std::string* o_query)
{
    /*
     * Build Query
     */
    std::string body;
    SQLEscape(a_body, body);

    std::stringstream ss;
    ss << "SELECT response,http_status FROM jsonapi('PATCH','" << a_uri << "','" << body <<  "','" << user_id_ << "', '" << entity_id_ << "', '" << entity_schema_ << "', '" << sharded_schema_ << "', '" << subentity_schema_ << "', '" << subentity_prefix_ << "');";

    /*
     * Run query ( asynchronously )...
     */
    AsyncQuery(a_loggable_data, ss.str(), a_callback, o_query);
}

/**
 * @brief An API to DELETE data using a subrequest that will be processed CASPER CONNECTORS POSTGRES module.
 *
 * @param a_uri
 * @param a_body
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Delete (const std::string& a_uri, const std::string& a_body, ::ev::postgresql::JSONAPI::Callback a_callback,
                                        std::string* o_query)
{
    Delete(loggable_data_ref_, a_uri, a_body, a_callback, o_query);
}

/**
 * @brief An API to DELETE data using a subrequest that will be processed CASPER CONNECTORS POSTGRES module.
 *
 * @param a_loggable_data
 * @param a_logger_module
 * @param a_uri
 * @param a_body
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::Delete (const ::ev::Loggable::Data& a_loggable_data,
                                        const std::string& a_uri, const std::string& a_body, ::ev::postgresql::JSONAPI::Callback a_callback,
                                        std::string* o_query)
{
    /*
     * Build Query
     */
    std::string body;
    SQLEscape(a_body, body);

    std::stringstream ss;
    ss << "SELECT response,http_status FROM jsonapi('DELETE','" << a_uri << "','" << body <<  "','" << user_id_ << "', '" << entity_id_ << "', '" << entity_schema_ << "', '" << sharded_schema_ << "', '" << subentity_schema_ << "', '" << subentity_prefix_ << "');";
    
    /*
     * Run query ( asynchronously )...
     */
    AsyncQuery(a_loggable_data, ss.str(), a_callback, o_query);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Issue a PG request, using a task.
 *
 * @param a_loggable_data
 * @param a_query
 * @param a_callback
 *
 * @param o_query
 */
void ::ev::postgresql::JSONAPI::AsyncQuery (const ::ev::Loggable::Data& a_loggable_data,
                                            const std::string a_query, ::ev::postgresql::JSONAPI::Callback a_callback,
                                            std::string* o_query)
{
    const std::string query = a_query;
    if ( nullptr != o_query ) {
        (*o_query) = query;
    }
    
    NewTask([this, query, a_loggable_data] () -> ::ev::Object* {
        
        return new ::ev::postgresql::Request(a_loggable_data, query);
        
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
        
    })->Finally([this, query, a_callback] (::ev::Object* a_object) {
        
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
        if ( 1 != rows_count && 2 != columns_count ) {
            throw ::ev::Exception("Unexpected PostgreSQL unexpected number of returned rows : got %dx%d, expected 1x2 ( rows x columns )!",
                                  rows_count, columns_count);
        }
        
        uint16_t status = static_cast<uint16_t>(atoi(value.raw_value(0, 1)));;
        
        a_callback(/* a_uri */ query.c_str(), /* a_json */ value.raw_value(/* a_row */ 0, /* a_column */0), /* a_error */ nullptr, /* a_status */ status, reply->elapsed_);
        
    })->Catch([query, a_callback] (const ::ev::Exception& a_ev_exception) {
        
        a_callback(/* a_uri */ query.c_str(), /* a_json */ nullptr, /* a_error */ a_ev_exception.what(), /* a_status */ 500, /* a_elapsed */ 0);
 
    });
}

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* ::ev::postgresql::JSONAPI::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}

/**
 * @brief Invalidate possible running tasks.
 */
void ::ev::postgresql::JSONAPI::InvalidateHandler ()
{
   ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
   ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Escape an SQL term.
 *
 * @param a_value
 * @param o_value
 */
void ::ev::postgresql::JSONAPI::SQLEscape (const std::string& a_value, std::string& o_value)
{
    o_value = "";
    const size_t count = a_value.size();
    if ( 0 == count ) {
        return;
    }
    for ( size_t idx = 0 ; idx < a_value.size(); ++idx ) {
        if ( '\'' == a_value[idx] ) {
            o_value += "''";
        } else {
            o_value += a_value[idx];
        }
    }
}
