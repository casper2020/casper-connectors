/**
 * @file http.cc
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

#include "ev/curl/http.h"

#include "ev/curl/reply.h"
#include "ev/curl/error.h"

/**
 * @brief Default constructor.
 */
ev::curl::HTTP::HTTP (const ev::Loggable::Data& a_loggable_data)
    : loggable_data_(a_loggable_data)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor..
 */
ev::curl::HTTP::~HTTP ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Perforn an HTTP GET request.
 *
 * @param a_url
 * @param a_headers
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::curl::HTTP::GET (const std::string& a_url,
                          const EV_CURL_HTTP_HEADERS* a_headers,
                          EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    Async(new ::ev::curl::Request(loggable_data_,
                                  curl::Request::HTTPRequestType::GET, a_url, a_headers, /* a_body */ nullptr
          ),
          a_success_callback, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP PUT request.
 *
 * @param a_url
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::curl::HTTP::PUT (const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                          const std::string* a_body,
                          EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    Async(new ::ev::curl::Request(loggable_data_,
                                  curl::Request::HTTPRequestType::PUT, a_url, a_headers, a_body
          ),
          a_success_callback, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP POST request.
 *
* @param a_url
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::curl::HTTP::POST (const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                           const std::string* a_body,
                           EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    Async(new ::ev::curl::Request(loggable_data_,
                                  curl::Request::HTTPRequestType::POST, a_url, a_headers, a_body
          ),
          a_success_callback, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP DELETE request.
 *
 * @param a_url
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::curl::HTTP::DELETE (const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                             const std::string* a_body,
                             EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    Async(new ::ev::curl::Request(loggable_data_,
                                  curl::Request::HTTPRequestType::DELETE, a_url, a_headers, a_body
          ),
          a_success_callback, a_failure_callback
    );
}

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* ::ev::curl::HTTP::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Perforn an HTTP request.
 *
 * @param a_request
 * @param a_success_callback
 * @param a_failure_callback
 */
void ::ev::curl::HTTP::Async (::ev::curl::Request* a_request,
                              EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    NewTask([a_request] () -> ::ev::Object* {

        return a_request;

    })->Then([] (::ev::Object* a_object) -> ::ev::Object* {

        ::ev::Result* result = dynamic_cast<::ev::Result*>(a_object);
        if ( nullptr == result ) {
            throw ::ev::Exception("Unexpected CURL result object: nullptr!");
        }

        const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(result->DataObject());
        if ( nullptr == reply ) {
            const ::ev::curl::Error* error = dynamic_cast<const ::ev::curl::Error*>(result->DataObject());
            if ( nullptr != error ) {
                throw ::ev::Exception(error->message());
            } else {
                throw ::ev::Exception("Unexpected CURL reply object: nullptr!");
            }
        }

        // ... same as reply, but it was detached ...
        return result->DetachDataObject();

    })->Finally([a_success_callback] (::ev::Object* a_object) {

        const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(a_object);
        if ( nullptr == reply ) {
            throw ::ev::Exception("Unexpected CURL data object!");
        }

        const ::ev::curl::Value& value = reply->value();

        if ( value.code() < 0 ) {
            throw ::ev::Exception("CURL error code: %d!", -1 * value.code());
        }

        a_success_callback(value);

    })->Catch([a_failure_callback] (const ::ev::Exception& a_ev_exception) {

        a_failure_callback(a_ev_exception);

    });
}
