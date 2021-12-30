/**
 * @file base.cc
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/easy/http/base.h"

#include "cc/macros.h"

#include "cc/b64.h"

#include "ev/curl/http.h" // for static helpers

#include "cc/easy/json.h"

#include <map>

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data TO BE COPIED
 * @param a_user_agent    User-Agent header value.
 */
cc::easy::http::Base::Base (const ev::Loggable::Data& a_loggable_data, const char* const a_user_agent)
 : loggable_data_(a_loggable_data), user_agent_(nullptr != a_user_agent ? a_user_agent : "")
{
    should_redact_        = true;
    follow_location_      = false;
    scheduler_client_ptr_ = nullptr;
}

/**
 * @brief Destructor.
 */
cc::easy::http::Base::~Base ()
{
    if ( nullptr != scheduler_client_ptr_ ) {
        ::ev::scheduler::Scheduler::GetInstance().Unregister(scheduler_client_ptr_);
    }
}


// MARK: -

/**
 * @brief Perform an HTTP HEAD request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_timeouts See \link http::http::Base::Timeouts \link.
 */
void cc::easy::http::Base::HEAD (const std::string& a_url, const http::Base::Headers& a_headers,
                                 http::Base::Callbacks a_callbacks, const http::Base::Timeouts* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::HEAD, a_url, a_headers, /* a_body */ nullptr, a_callbacks, a_timeouts);
}

/**
 * @brief Perform an HTTP GET request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_timeouts See \link http::http::Base::Timeouts \link.
 */
void cc::easy::http::Base::GET (const std::string& a_url, const http::Base::Headers& a_headers,
                                http::Base::Callbacks a_callbacks, const http::Base::Timeouts* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::GET, a_url, a_headers, /* a_body */ nullptr, a_callbacks, a_timeouts);
}

/**
 * @brief Perform an HTTP PUT request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 *
 * @param a_timeouts See \link http::Base::ClientTimeouts \link.
 */
void cc::easy::http::Base::PUT (const std::string& a_url, const http::Base::Headers& a_headers, const std::string& a_body,
                                http::Base::Callbacks a_callbacks, const http::Base::Timeouts* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::PUT, a_url, a_headers, &a_body, a_callbacks, a_timeouts);
}

/**
 * @brief Perform an HTTP POST request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_timeouts  See \link http::http::Base::Timeouts \link.
 */
void cc::easy::http::Base::POST (const std::string& a_url, const http::Base::Headers& a_headers, const std::string& a_body,
                                 http::Base::Callbacks a_callbacks, const http::Base::Timeouts* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::POST, a_url, a_headers, &a_body, a_callbacks, a_timeouts);
}

/**
 * @brief Perform an HTTP PATCH request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_timeouts  See \link http::http::Base::Timeouts \link.
 */
void cc::easy::http::Base::PATCH (const std::string& a_url, const http::Base::Headers& a_headers, const std::string& a_body,
                                  http::Base::Callbacks a_callbacks, const http::Base::Timeouts* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::PATCH, a_url, a_headers, &a_body, a_callbacks, a_timeouts);
}

/**
 * @brief Perform an HTTP DELETE request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation ( optional ).
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_timeouts  See \link http::http::Base::Timeouts \link.
 */
void cc::easy::http::Base::DELETE (const std::string& a_url, const http::Base::Headers& a_headers, const std::string* a_body,
                                   http::Base::Callbacks a_callbacks, const http::Base::Timeouts* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::DELETE, a_url, a_headers, a_body, a_callbacks, a_timeouts);
}


// MARK: - [PRIVATE] - Helper Method(s)

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* cc::easy::http::Base::NewTask (const EV_TASK_PARAMS& a_callback)
{
    CC_DEBUG_ASSERT(nullptr != scheduler_client_ptr_);
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(scheduler_client_ptr_, a_task);
                                     }
    );
}

/**
 * @brief Perform an HTTP request.
 *
 * @param a_request  Request to execute.
 * @param a_then     Set of functions to call on sequence.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                    If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                    otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 */
void cc::easy::http::Base::Async (::ev::curl::Request* a_request, const std::vector<EV_TASK_CALLBACK> a_then,
                                  http::Base::Callbacks a_callbacks)
{
    CC_DEBUG_ASSERT(nullptr != scheduler_client_ptr_);
    
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, url   , a_request->url());
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, token , CC_QUALIFIED_CLASS_NAME(this));
    const std::string id     = ::cc::ObjectHexAddr<::ev::curl::Request>(a_request);
    const std::string method = a_request->method();

    try {
        if ( true == follow_location_ ) {
            a_request->SetFollowLocation();
        }
        if ( 0 != user_agent_.length() ) {
            a_request->SetUserAgent(user_agent_);
        }
        CC_IF_DEBUG(
            if ( nullptr != cURLed_callbacks_.debug_ ) {
                a_request->EnableDebug(cURLed_callbacks_.debug_);
            }
            if ( nullptr != cURLed_callbacks_.progress_ ) {
                a_request->EnableDebugProgress(cURLed_callbacks_.progress_);
            }
        )
    } catch (...) {
        delete a_request;
        cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
    
    // ... prepare task ...
    ::ev::scheduler::Task* t = NewTask([CC_IF_DEBUG(token, )a_request, id, this] () -> ::ev::Object* {
        // ... log request?
        if ( nullptr != cURLed_callbacks_.log_request_ ) {
            cURLed_callbacks_.log_request_(*a_request, ::ev::curl::HTTP::cURLRequest(id, a_request, should_redact_));
        }
        // ... dump request ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, a_request););
        // ... perform request ...
        return a_request;
    });
    // ... chain events ...
    for ( const auto& then : a_then ) {
        t->Then(then);
    }
    // ... common closure ...
    t->Finally([CC_IF_DEBUG(token, url,)this, a_callbacks, id, method]  (::ev::Object* a_object) {
        if ( nullptr == a_callbacks.on_error_ ) {
            // ... no error handler ...
            const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(a_object);
            // ... if not a 'reply' it must be a 'result' object ...
            if ( nullptr == reply ) {
                reply = EnsureReply(a_object);
            }
            const ::ev::curl::Value& value = reply->value();
            // ... log response?
            if ( nullptr != cURLed_callbacks_.log_response_ ) {
                cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), should_redact_));
            }
            // ... dump response ...
            CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, url, value););
            // ... notify ...
            a_callbacks.on_success_(value);
        } else {
            // ... if not a 'reply' it must be a 'result' object ...
            const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(a_object);
            if ( nullptr != reply ) {
                const ::ev::curl::Value& value = reply->value();
                // ... log response?
                if ( nullptr != cURLed_callbacks_.log_response_ ) {
                    cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), should_redact_));
                }
                // ... dump response ...
                CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, url, value););
                // ... notify ...
                a_callbacks.on_success_(value);
            } else {
                // ... must be a result!
                const ev::Result* result = dynamic_cast<const ::ev::Result*>(a_object);
                if ( nullptr == result ) {
                    throw ::ev::Exception("Unexpected CURL result object: nullptr!");
                }
                // ... can be a reply or an error ...
                const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(result->DataObject());
                if ( nullptr != reply ) {
                    const ::ev::curl::Value& value = reply->value();
                    // ... log response?
                    if ( nullptr != cURLed_callbacks_.log_response_ ) {
                        cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), should_redact_));
                    }
                    // ... dump response ...
                    CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, url, value););
                    // ... notify ...
                    a_callbacks.on_success_(value);
                } else {
                    // ... error?
                    const ::ev::curl::Error* error = dynamic_cast<const ::ev::curl::Error*>(result->DataObject());
                    if ( nullptr != error ) {
                        a_callbacks.on_error_(*error);
                    } else {
                        throw ::ev::Exception("Unexpected CURL reply object: nullptr!");
                    }
                }
            }
        }
    })->Catch([CC_IF_DEBUG(token, id, method, url,)a_callbacks] (const ::ev::Exception& a_ev_exception) {
        // ... dump response ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpException(token, id, method, url, a_ev_exception););
        // ... notify ...
        a_callbacks.on_failure_(a_ev_exception);
    });
}

// MARK: - [STATIC] - Helper Method(s)

/**
 * @brief Ensure a valid HTTP reply.
 *
 * @param a_object Test subject.
 */
const ::ev::Result* cc::easy::http::Base::EnsureResult (::ev::Object* a_object)
{
    ::ev::Result* result = dynamic_cast<::ev::Result*>(a_object);
    if ( nullptr == result ) {
        throw ::ev::Exception("Unexpected CURL result object: nullptr!");
    }
    return result;
}

/**
 * @brief Ensure a valid HTTP reply.
 *
 * @param a_object Test subject.
 */
const ::ev::curl::Reply* cc::easy::http::Base::EnsureReply (::ev::Object* a_object)
{
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
    return reply;
}

// MARK: - [STATIC] - Helper Method(s)

/**
 * @brief Fill an URL with a query from params map.
 *
 * @param a_url    Base URL.
 * @param a_params Parameters map.
 * @param a_url    Final URL.
 */
void cc::easy::http::Base::SetURLQuery (const std::string& a_url, const std::map<std::string, std::string>& a_params, std::string& o_url)
{
    o_url = a_url;
    // ... nothing to do?
    if ( 0 == a_params.size() ) {
        // ... done ...
        return;;
    }
    // ... prepare 'escape' helper ...
    CURL *curl = curl_easy_init();
    if ( nullptr == curl ) {
        throw ::ev::Exception("Unexpected cURL handle: nullptr!");
    }
    // ... escape and append all params ...
    char* escaped = nullptr;
    try {
        // ... first ...
        const auto& first = a_params.begin();
        {
            char* escaped = curl_easy_escape(curl, first->second.c_str(), static_cast<int>(first->second.length()));
            o_url += "?" + first->first + '=' + escaped;
            if ( nullptr == escaped ) {
                curl_easy_cleanup(curl);
                throw ::ev::Exception("Unexpected cURL easy escape: nullptr!");
            }
            curl_free(escaped);
        }
        // ... all other ...
        for ( auto param = ( std::next(first) ) ; a_params.end() != param ; ++param ) {
            if ( 0 == param->second.length() ) {
                continue;
            }
            escaped = curl_easy_escape(curl, param->second.c_str(), static_cast<int>(param->second.length()));
            if ( nullptr == escaped ) {
                curl_easy_cleanup(curl);
                throw ::ev::Exception("Unexpected cURL easy escape: nullptr!");
            }
            o_url += "&" + param->first + "=" + std::string(escaped);
            curl_free(escaped);
            escaped = nullptr;
        }
        // ... cleanup ...
        curl_easy_cleanup(curl);
    } catch (...) {
        // ... cleanup ...
        if ( nullptr != escaped ) {
            curl_free(escaped);
        }
        curl_easy_cleanup(curl);
        // ... notify ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
}
