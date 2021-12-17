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

#include <iomanip> // std::left, std::setfill, etc...

#include "cc/macros.h"
#include "cc/debug/types.h"
#include "cc/easy/json.h"

/**
 * @brief Default constructor.
 */
ev::curl::HTTP::HTTP ()
: cURLed_redact_(true)
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
 * @param a_loggable_data
 * @param a_url              URL
 * @param a_headers          HTTP Headers.
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
 void ev::curl::HTTP::GET (const Loggable::Data& a_loggable_data,
                          const std::string& a_url,
                          const EV_CURL_HTTP_HEADERS* a_headers,
                          EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                           const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    Async(new ::ev::curl::Request(a_loggable_data,
                                  curl::Request::HTTPRequestType::GET, a_url, a_headers, /* a_body */ nullptr, a_timeouts
          ),
          a_success_callback, nullptr, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP GET request.
 *
 * @param a_loggable_data
 * @param a_url              URL
 * @param a_headers          HTTP Headers.
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_uri              Local file URI where response will be written to.
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
void ev::curl::HTTP::GET (const Loggable::Data& a_loggable_data,
                          const std::string& a_url,
                          const EV_CURL_HTTP_HEADERS* a_headers,
                          EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                          const std::string& a_uri,
                          const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    ::ev::curl::Request* request = new ::ev::curl::Request(a_loggable_data,
                                                           curl::Request::HTTPRequestType::GET, a_url, a_headers, /* a_body */ nullptr, a_timeouts
    );
    request->SetWriteResponseBodyTo(a_uri);
    Async(request,
          a_success_callback, nullptr, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP PUT request.
 *
 * @param a_url              URL
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
void ev::curl::HTTP::PUT (const Loggable::Data& a_loggable_data,
                          const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                          const std::string* a_body,
                          EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                          const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    Async(new ::ev::curl::Request(a_loggable_data,
                                  curl::Request::HTTPRequestType::PUT, a_url, a_headers, a_body, a_timeouts
          ),
          a_success_callback, nullptr, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP POST request.
 *
 * @param a_loggable_data
 * @param a_url              URL
 * @param a_headers          HTTP Headers.
 * @param a_body             Body to send.
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_error_callback   Function to call when an error ocurred  ( REQUEST NOT EXECUTED ).
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
 void ev::curl::HTTP::POST (const Loggable::Data& a_loggable_data,
                            const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                            const std::string* a_body,
                            EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_ERROR_CALLBACK a_error_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                            const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    Async(new ::ev::curl::Request(a_loggable_data,
                                  curl::Request::HTTPRequestType::POST, a_url, a_headers, a_body, a_timeouts
          ),
          a_success_callback, a_error_callback, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP POST request.
 *
 * @param a_loggable_data
 * @param a_uri              Local file URI where body will be read from.
 * @param a_url              URL
 * @param a_headers          HTTP Headers.
 * @param a_body             Body to send.
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
void ev::curl::HTTP::POST (const Loggable::Data& a_loggable_data,
                           const std::string& a_uri,
                           const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                           EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                           const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    ::ev::curl::Request* request = new ::ev::curl::Request(a_loggable_data,
                                                           curl::Request::HTTPRequestType::POST, a_url, a_headers, /* a_body */ nullptr, a_timeouts
    );
    request->SetReadBodyFrom(a_uri);
    Async(request,
          a_success_callback, nullptr, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP PATCH request.
 *
 * @param a_loggable_data
 * @param a_uri              Local file URI where body will be read from.
 * @param a_url              URL
 * @param a_headers          HTTP Headers.
 * @param a_body             Body to send.
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
void ev::curl::HTTP::PATCH (const ::ev::Loggable::Data& a_loggable_data,
                            const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                            const std::string* a_body,
                            EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                            const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    Async(new ::ev::curl::Request(a_loggable_data,
                                  curl::Request::HTTPRequestType::PATCH, a_url, a_headers, a_body, a_timeouts
          ),
          a_success_callback, nullptr, a_failure_callback
    );
}

/**
 * @brief Perforn an HTTP DELETE request.
 *
 * @param a_loggable_data
 * @param a_uri              Local file URI where body will be read from.
 * @param a_url              URL
 * @param a_headers          HTTP Headers.
 * @param a_body             Body to send.
 * @param a_success_callback Function to call on success ( REQUEST EXECUTED, DOES NOT MEAN THE GOT A 200 FAMILY STATUS CODE ).
 * @param a_failure_callback Function to call when an exception was raised ( REQUEST NOT EXECUTED ).
 * @param a_timeouts         See \link EV_CURL_HTTP_TIMEOUTS \link
 */
void ev::curl::HTTP::DELETE (const Loggable::Data& a_loggable_data,
                             const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                             const std::string* a_body,
                             EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                             const EV_CURL_HTTP_TIMEOUTS* a_timeouts)
{
    Async(new ::ev::curl::Request(a_loggable_data,
                                  curl::Request::HTTPRequestType::DELETE, a_url, a_headers, a_body, a_timeouts
          ),
          a_success_callback, nullptr, a_failure_callback
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
 * @param a_error_callback
 * @param a_failure_callback
 */
void ::ev::curl::HTTP::Async (::ev::curl::Request* a_request,
                              EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_ERROR_CALLBACK a_error_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback)
{
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, url   , a_request->url());
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, token , CC_QUALIFIED_CLASS_NAME(this));

    const std::string id     = CC_OBJECT_HEX_ADDR(a_request);
    const std::string method = a_request->method();

    NewTask([CC_IF_DEBUG(token, )id, a_request, this] () -> ::ev::Object* {

        // ... log request?
        if ( nullptr != cURLed_callbacks_.log_request_ ) {
            cURLed_callbacks_.log_request_(*a_request, cURLRequest(id, a_request, cURLed_redact_));
        }
        // ... dump request ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, a_request););
        
        // ...
        return a_request;

    })->Then([a_error_callback CC_IF_DEBUG(,token, url), id, method, this] (::ev::Object* a_object) -> ::ev::Object* {

        ::ev::Result* result = dynamic_cast<::ev::Result*>(a_object);
        if ( nullptr == result ) {
            throw ::ev::Exception("Unexpected CURL result object: nullptr!");
        }

        const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(result->DataObject());
        if ( nullptr == reply ) {
            const ::ev::curl::Error* error = dynamic_cast<const ::ev::curl::Error*>(result->DataObject());
            if ( nullptr != error ) {
                // ... notify or throw?
                if ( nullptr != a_error_callback ) {
                    // ... notify ...
                    a_error_callback(*error);
                    // ... done here ...
                    return nullptr;
                } else {
                    // ... throw exception with error message ...
                    throw ::ev::Exception(error->message());
                }
            } else {
                // ... throw exception ...
                throw ::ev::Exception("Unexpected CURL reply object: nullptr!");
            }
        }
        // ... log response?
        if ( nullptr != cURLed_callbacks_.log_response_ ) {
            cURLed_callbacks_.log_response_(reply->value(), cURLResponse(id, method, reply->value(), cURLed_redact_));
        }
        // ... dump response ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method.c_str(), url, reply->value()););
        // ... same as reply, but it was detached ...
        return result->DetachDataObject();

    })->Finally([a_error_callback, a_success_callback] (::ev::Object* a_object) {
        
        // ... error set and was already reported?
        if ( nullptr == a_object && nullptr != a_error_callback  ) {
            // ... done here ...
            return;
        }

        const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(a_object);
        if ( nullptr == reply ) {
            throw ::ev::Exception("Unexpected CURL data object!");
        }

        const ::ev::curl::Value& value = reply->value();

        if ( value.code() < 0 ) {
            throw ::ev::Exception("CURL error code: %d!", -1 * value.code());
        }

        a_success_callback(value);

    })->Catch([CC_IF_DEBUG(id, token, url, method,)a_failure_callback] (const ::ev::Exception& a_ev_exception) {

        // ... dump response ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpException(token, id, method.c_str(), url, a_ev_exception););

        // ... notify ...
        a_failure_callback(a_ev_exception);

    });
}

// MARK: - DEBUG ONLY: Helper Method(s) / Function(s)

#if defined(CC_DEBUG_ON)
            
/**
 * @brief Call this method to dump an HTTP request data before it's execution.
 *
 * @param a_token   Debug token.
 * @param a_id      ID.
 * @param a_request Request object.
 */
void ::ev::curl::HTTP::DumpRequest (const std::string& a_token, const std::string& a_id, const ::ev::curl::Request* a_request)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    const char* const token_c_str = a_token.c_str();
    
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '-').c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%s // %s // %s // %s\n", a_id.c_str(), a_token.c_str(), a_request->method(), "REQUEST");
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '-').c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "ID", a_id.c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "Method", a_request->method());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "URL", a_request->url().c_str());
    if ( a_request->tx_headers().size() > 0 ) {
        CC_DEBUG_LOG_PRINT(token_c_str, "%-7s:\n", "Headers");
        std::stringstream ss;
        for ( auto header : a_request->tx_headers() ) {
            ss.str("");
            ss << '\t' << std::right << std::setfill(' ') << std::setw(25) << header.first << ": ";
            for ( auto value : header.second ) {
                ss << value << ' ';
            }
            CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", ss.str().c_str());
        }
    }
    if ( 0 != strcasecmp(a_request->method(), "GET") ) {
        // ... JSON?
        if ( 0 == strncasecmp("application/json", a_request->tx_header_value("content-type").c_str(), sizeof(char)* 16) ) {
            try {
                const ::cc::easy::JSON<::ev::Exception> json; Json::Value tmp; json.Parse(a_request->tx_body().c_str(),tmp);
                CC_DEBUG_LOG_PRINT(token_c_str, "%-7s:\n%s", "Body", tmp.toStyledString().c_str());
            } catch (...) {
                CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s, " SIZET_FMT " byte(s)\n", "Body",
                                   a_request->tx_body().length() ? "redacted" : "empty", a_request->tx_body().length()
                );
            }
        } else {
            CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s, " SIZET_FMT " byte(s)\n", "Body",
                               a_request->tx_body().length() ? "redacted" : "empty", a_request->tx_body().length()
            );
        }
    }
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s:\n", "Timeouts");
    CC_DEBUG_LOG_PRINT(token_c_str, "\t%13s: %ld\n", "Connection", a_request->timeouts().connection_);
    CC_DEBUG_LOG_PRINT(token_c_str, "\t%13s: %ld\n", "Operation" , a_request->timeouts().operation_);
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '.').c_str());
}

/**
 * @brief Call this method to dump an HTTP response value.
 *
 * @param a_token  Debug token.
 * @param a_id     ID.
 * @param a_method Method name.
 * @param a_url    URL.
 * @param a_value  Value object.
 */
void ::ev::curl::HTTP::DumpResponse (const std::string& a_token, const std::string& a_id, const std::string& a_method, const std::string& a_url,
                                     const ::ev::curl::Value& a_value)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    const char* const token_c_str = a_token.c_str();
    
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '-').c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%s // %s // %s // %s\n", a_id.c_str(), a_token.c_str(), a_method.c_str(), "RESPONSE");
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '-').c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "ID", a_id.c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "Method", a_method.c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "URL", a_url.c_str());
    if ( a_value.headers().size() > 0 ) {
        CC_DEBUG_LOG_PRINT(token_c_str, "%-7s:\n", "Headers");
        std::stringstream ss;
        for ( auto header : a_value.headers() ) {
            ss.str("");
            ss << '\t' << std::right << std::setfill(' ') << std::setw(25) << header.first << ": ";
            for ( auto value : header.second ) {
                ss << value << ' ';
            }
            CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", ss.str().c_str());
        }
    }
    // ... JSON?
    if ( 0 == strncasecmp("application/json", a_value.header_value("content-type").c_str(), sizeof(char)* 16) ) {
        const ::cc::easy::JSON<::ev::Exception> json; Json::Value tmp; json.Parse(a_value.body(),tmp);
        CC_DEBUG_LOG_PRINT(token_c_str, "%-7s:\n%s", "Body", tmp.toStyledString().c_str());
    } else {
        CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: " SIZET_FMT " byte(s), %s\n", "Body", a_value.body().length(), a_value.body().length() ? "redacted" : "empty");
    }
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %d\n", "Status", a_value.code());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: " SIZET_FMT "ms\n", "RTT", a_value.rtt());
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '.').c_str());
}

/**
 * @brief Call this method to dump an HTTP response error data.
 *
 * @param a_token     Debug token.
 * @param a_id        ID.
 * @param a_method    Method name.
 * @param a_url       URL.
 * @param a_exception Exception object.
 */
void ::ev::curl::HTTP::DumpException (const std::string& a_token, const std::string& a_id, const std::string& a_method, const std::string& a_url,
                                      const ::ev::Exception& a_exception)
{
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    const char* const token_c_str = a_token.c_str();
    
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '-').c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%s // %s // %s // %s\n", a_id.c_str(), a_token.c_str(), a_method.c_str(), "EXCEPTION");
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '-').c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "ID", a_id.c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "Method", a_method.c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "URL", a_url.c_str());
    CC_DEBUG_LOG_PRINT(token_c_str, "%-7s: %s\n", "Exception", a_exception.what());
    CC_DEBUG_LOG_PRINT(token_c_str, "%s\n", std::string(80, '.').c_str());
}

#endif // defined(CC_DEBUG_ON)

/**
 * @brief Call this method to dump ( via callback ) an HTTP request data before it's execution.
 *
 * @param a_id      ID.
 * @param a_request Request object.
 * @param a_redact  Try to redact sensitive data when possible.
 */
std::string ev::curl::HTTP::cURLRequest (const std::string& a_id, const ::ev::curl::Request* a_request, const bool a_redact)
{
    std::stringstream ss;
    // ... cmd ...
    ss << "## " << a_id << " @ " << ::cc::UTCTime::NowISO8601DateTime() << " // " << a_request->method() << " // REQUEST" << '\n';
    ss << "curl";
    // ...method?
    ss << " -X '" << a_request->method() << "' \\\n";
    // ... headers ...
    const auto& headers = a_request->tx_headers();
    if ( headers.size() > 0 ) {
        std::stringstream tmp;
        for ( auto header : headers ) {
            tmp << header.first << ": ";
            if ( false == a_redact || ( true == a_redact && not ( 0 == strcasecmp(header.first.c_str(), "Authorization") ) ) ) {
                for ( auto idx = 0 ; idx < header.second.size(); ++idx ) {
                    tmp << header.second[idx];
                    if ( idx < ( header.second.size() - 1 ) ) {
                        tmp << ' ';
                    }
                }
            } else {
                tmp << "<redacted>";
            }
            ss << "     -H '" << tmp.str() << "' \\\n";
            tmp.str("");
        }
    }
    // ... body?
    const auto& body = a_request->tx_body();
    if ( body.length() > 0 ) {
        // ... redact ...
        if ( true == a_redact ) {
            if ( true == ::cc::easy::JSON<::ev::Exception>::IsJSON(a_request->tx_header_value("Content-Type"))
                &&
                ( 0 != strcasestr(body.c_str(), "token_") || 0 != strcasestr(body.c_str(), "password") )
            ) {
                const ::cc::easy::JSON<::ev::Exception> json;
                try {
                    Json::Value object;
                    json.Parse(body, object);
                    json.Redact({ "password", "access_token", "refresh_token"}, object);
                    // ... write redacted object ...
                    ss << "     -d $'" << json.Write(object) << "' \\\n";
                } catch (...) {
                    // ... don't know how to partially redact it, so redact it all ...
                    ss << "     -d $'<redacted>' \\\n";
                }
            } else {
                // ... don't know how to partially redact it, so redact it all ...
                ss << "     -d $'<redacted>' \\\n";
            }
        } else {
            // ... NOT redacted ...
            ss << "     -d $'" << a_request->tx_body() << "' \\\n";
        }
    }
    // ... timeouts ...
    if ( -1 != a_request->timeouts().connection_ ) {
        ss << "     --connect-timeout " << a_request->timeouts().connection_ << " \\\n";
    }
    if ( -1 != a_request->timeouts().operation_ ) {
        ss << "     --max-time " << a_request->timeouts().operation_ << " \\\n";
    }
    // ... url ...
    ss << " '" << a_request->url() << "'";
    // ... done ...
    return ss.str();
}

/**
 * @brief Call this method to dump ( via callback ) an HTTP response value.
 *
 * @param a_id     ID.
 * @param a_method Method name.
 * @param a_value  Value object.
 * @param a_redact Try to redact sensitive data when possible.
 */
std::string ev::curl::HTTP::cURLResponse (const std::string& a_id, const std::string& a_method,
                                          const ::ev::curl::Value& a_value, const bool a_redact)
{
    std::stringstream ss;
    // ...
    ss << "## " << a_id << " @ " << ::cc::UTCTime::NowISO8601DateTime() << " // " << a_method << " // RESPONSE" << '\n';
    // ... method & url ...
    ss << "> " << a_method << ' ' << a_value.url() << '\n';
    // ... headers ...
    ss << "< HTTP/" << a_value.http_version() << ' ' << a_value.code() << '\n';
    for ( const auto& header : a_value.headers() ) {
        ss << "< " << header.first << ":";
        for ( const auto& value : header.second ) {
            ss << ' ' << value;
        }
        ss << '\n';
    }
    // ... redact ...
    if ( true == a_redact ) {
        if ( true == ::cc::easy::JSON<::ev::Exception>::IsJSON(a_value.header_value("Content-Type"))
            &&
            ( 0 != strcasestr(a_value.body().c_str(), "token_") || 0 != strcasestr(a_value.body().c_str(), "password") )
        ) {
            const ::cc::easy::JSON<::ev::Exception> json;
            try {
                Json::Value object;
                json.Parse(a_value.body(), object);
                json.Redact({ "password", "access_token", "refresh_token"}, object);
                // ... write redacted object ...
                ss << json.Write(object);
            } catch (...) {
                // ... don't know how to partially redact it, so redact it all ...
                ss << "<redacted>";
            }
        } else {
            // ... don't know how to partially redact it, so redact it all ...
            ss << "<redacted>";
        }
    } else {
        // ... NOT redacted ...
        ss << a_value.body();
    }
    // ... done ...
    return ss.str();
}
