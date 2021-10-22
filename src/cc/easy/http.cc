/**
 * @file http.cc
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-fs-assistant.
 *
 * casper-fs-assistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-fs-assistant  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-fs-assistant.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/easy/http.h"

#include "cc/macros.h"

#include "cc/b64.h"

#include "ev/curl/error.h"

#include "cc/easy/json.h"

#include <map>

// MARK: - HTTPClient

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data TO BE COPIED
 */
cc::easy::HTTPClient::HTTPClient (const ev::Loggable::Data& a_loggable_data)
 : loggable_data_(a_loggable_data)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::easy::HTTPClient::~HTTPClient ()
{
    /* empty */
}

/**
 * @brief Perform an HTTP GET request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 */
void cc::easy::HTTPClient::GET (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                                 HTTPClient::Callbacks a_callbacks)
{
    http_.GET(loggable_data_,
               a_url, &a_headers,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {
                    a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               }
    );
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
 */
void cc::easy::HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                 HTTPClient::Callbacks a_callbacks)
{
    http_.POST(loggable_data_,
               a_url, &a_headers, &a_body,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {
                    a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               }
    );
}

// MARK: - OAuth2HTTPClient

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data TO BE COPIED
 * @param a_config R/O reference to \link OAuth2HTTPClient::Config \link.
 * @param a_tokens R/W reference to \link OAuth2HTTPClient::Tokens \link.
 */
cc::easy::OAuth2HTTPClient::OAuth2HTTPClient (const ::ev::Loggable::Data& a_loggable_data, const OAuth2HTTPClient::Config& a_config, OAuth2HTTPClient::Tokens& a_tokens)
    : loggable_data_(a_loggable_data), config_(a_config), tokens_(a_tokens), http_(loggable_data_), nsi_ptr_(nullptr)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor.
 */
cc::easy::OAuth2HTTPClient::~OAuth2HTTPClient ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}

/**
 * @brief Perform an HTTP POST request to obtains tokens from an 'autorization code' grant flow.
 *
 * @param a_code     OAuth2 Auhtorization code.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 */
void cc::easy::OAuth2HTTPClient::AutorizationCodeGrant (const std::string& a_code,
                                                        OAuth2HTTPClient::POSTCallbacks a_callbacks)
{
    CURL *curl = curl_easy_init();
    if ( nullptr == curl ) {
        throw ::ev::Exception("Unexpected cURL handle: nullptr!");
    }

    std::string url = config_.oauth2_.urls_.token_ + "?grant_type=authorization_code";

    const std::map<std::string, std::string> map = {
        { "code"         , a_code                                      },
        { "client_id"    , config_.oauth2_.credentials_.client_id_     },
        { "client_secret", config_.oauth2_.credentials_.client_secret_ },
        { "redirect_uri" , config_.oauth2_.redirect_uri_               }
    };
    
    for ( auto param : map ) {
        char *output = curl_easy_escape(curl, param.second.c_str(), static_cast<int>(param.second.length()));
        if ( nullptr == output ) {
            curl_easy_cleanup(curl);
            throw ::ev::Exception("Unexpected cURL easy escape: nullptr!");
        }
        url += "&" + param.first + "=" + std::string(output);
        curl_free(output);
    }
    
    curl_easy_cleanup(curl);
    
    NewTask([this, url] () -> ::ev::Object* {
        
        return new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, url, nullptr, nullptr);
        
    })->Finally([this, a_callbacks] (::ev::Object* a_object) {

        const ::ev::curl::Reply* reply = EnsureReply(a_object);;
        const ::ev::curl::Value& value = reply->value();
        // ... notify ...
        a_callbacks.on_success_(value.code(), value.header_value("Content-Type"), value.body(), value.rtt());
        
    })->Catch([a_callbacks] (const ::ev::Exception& a_ev_exception) {
        a_callbacks.on_failure_(a_ev_exception);
    });    
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
 */
void cc::easy::OAuth2HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                       OAuth2HTTPClient::POSTCallbacks a_callbacks)
{
    // ... copy original header ...
    CC_HTTP_HEADERS headers;
    for ( auto it : a_headers ) {
        for ( auto it2 : it.second ) {
            headers[it.first].push_back(it2);
        }
    }
    // ... apply new access token ...
    headers["Authorization"].push_back("Bearer " + tokens_.access_);
    // ... notify interceptor?
    if ( nullptr != nsi_ptr_ ) {
        nsi_ptr_->OnHTTPRequestHeaderSet(headers);
    }
    // ... perform request ...
    ::ev::curl::Request* request = new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, a_url, &headers, &a_body);
    
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, url   , request->url());
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, method, request->method());
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, token , CC_QUALIFIED_CLASS_NAME(this));
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, id    , CC_OBJECT_HEX_ADDRESS(request));
    
    NewTask([CC_IF_DEBUG(token, id,)request] () -> ::ev::Object* {

        // ... dump request ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, request););

        // ... perform request ...
        return request;
        
    })->Then([CC_IF_DEBUG(token, id, method,)this, a_url] (::ev::Object* a_object) -> ::ev::Object* {
        
        const ::ev::curl::Reply* reply = EnsureReply(a_object);
        const ::ev::curl::Value& value = reply->value();
        
        // ... unauthorized?
        const bool unauthorized = ( 401 == value.code() || ( nullptr != nsi_ptr_ && nsi_ptr_->OnHTTPRequestReturned(value.code(), value.headers(), value.body()) ) );
        if ( true == unauthorized ) {
            // ... dump response ...
            CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, a_url, reply->value()););
            // ... notify interceptor?
            if ( nullptr != nsi_ptr_ ) {
                // ... abort refresh?
                if ( false == nsi_ptr_->OnUnauthorizedShouldRefresh(a_url, value.headers()) ) {
                    // ... yes ...
                    return dynamic_cast<::ev::Result*>(a_object)->DetachDataObject();
                }
            }
            // ... yes, refresh tokens now ...
            CC_HTTP_HEADERS headers = {
                { "Authorization", { "Basic " + ::cc::base64_url_unpadded::encode((config_.oauth2_.credentials_.client_id_ + ':' + config_.oauth2_.credentials_.client_secret_)) } },
                { "Content-Type" , { "application/x-www-form-urlencoded" } }
            };
            std::string body = "grant_type=refresh_token&refresh_token=" + ( tokens_.refresh_ );
            // ... notify interceptor?
            if ( nullptr != nsi_ptr_ ) {
                nsi_ptr_->OnOAuth2RequestSet(headers, body);
            }
            // ... new request ...
            ::ev::curl::Request* request = new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, config_.oauth2_.urls_.token_, &headers, &body);
            // ... dump request ...
            CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, request););
            // ... perform request ...
            return request;
        } else {
            // ... no, continue ..
            return dynamic_cast<::ev::Result*>(a_object)->DetachDataObject();
        }
        
    })->Then([CC_IF_DEBUG(token, id, method,)this, a_callbacks, a_url, a_headers, a_body] (::ev::Object* a_object) -> ::ev::Object* {
        
        const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(a_object);
        if ( nullptr != reply ) {
            // ... redirect to 'Finally' ...
            return a_object;
        } else {
            // ...  OAuth2 tokens refresh request response is expected ...
            const ::ev::curl::Reply* reply = EnsureReply(a_object);
            const ::ev::curl::Value& value = reply->value();
            // ... success?
            if ( 200 == value.code() ) {
                // ... dump response ...
                CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, a_url, reply->value()););
                // ... keep track of new tokens ...
                if ( nullptr != nsi_ptr_ ) {
                    nsi_ptr_->OnOAuth2RequestReturned(value.headers(), value.body(), tokens_.scope_, tokens_.access_, tokens_.refresh_, tokens_.expires_in_);
                } else {
                    // ... expecting JSON response ...
                    cc::easy::JSON<::ev::Exception> json;
                    Json::Value                     response;
                    json.Parse(value.body(), response);
                    // ... keep track of new tokens ...
                    tokens_.scope_      = json.Get(response, "scope"        , Json::ValueType::stringValue, /* a_default */ nullptr).asString();
                    tokens_.access_     = json.Get(response, "access_token" , Json::ValueType::stringValue, /* a_default */ nullptr).asString();
                    tokens_.refresh_    = json.Get(response, "refresh_token", Json::ValueType::stringValue, /* a_default */ nullptr).asString();
                    // ... optinal value, no default value ...
                    if ( true == response.isMember("expires_in") && response.isNumeric() ) {
                        tokens_.expires_in_ = response["expires_in"].asInt64();
                    } else {
                        tokens_.expires_in_ = 0;
                    }
                }
                // ... notify tokens changed ...
                if ( nullptr != tokens_.on_change_ ) {
                    tokens_.on_change_();
                }
                
                // ... copy original header ...
                CC_HTTP_HEADERS headers;
                for ( auto it : a_headers ) {
                    for ( auto it2 : it.second ) {
                        headers[it.first].push_back(it2);
                    }
                }
                // ... apply new access token ...
                headers["Authorization"].push_back("Bearer " + tokens_.access_);
                // ... notify interceptor?
                if ( nullptr != nsi_ptr_ ) {
                    nsi_ptr_->OnHTTPRequestHeaderSet(headers);
                }
                // ... new request ...
                ::ev::curl::Request* request = new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, a_url, &headers, &a_body);
                // ... dump request ...
                CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, request););
                // ... the original request must be performed again ...
                return request;
            } else {
                // ... redirect to 'Finally' with OAuth2 error response ...
                return a_object;
            }
        }
        
    })->Finally([CC_IF_DEBUG(token, id, method, url,)this, a_callbacks]  (::ev::Object* a_object) {
        
        const ::ev::curl::Reply* reply = dynamic_cast<const ::ev::curl::Reply*>(a_object);
        // ... if not a 'reply' it must be a 'result' object ...
        if ( nullptr == reply ) {
            reply = EnsureReply(a_object);
        }
        const ::ev::curl::Value& value = reply->value();
        // ... dump response ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, url, value););
        // ... notify ...
        a_callbacks.on_success_(value.code(), value.header_value("Content-Type"), value.body(), value.rtt());
        
    })->Catch([CC_IF_DEBUG(token, id, method, url,)a_callbacks] (const ::ev::Exception& a_ev_exception) {
        // ... dump response ...
        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpException(token, id, method, url, a_ev_exception););
        // ... notify ...
        a_callbacks.on_failure_(a_ev_exception);
    });
}

// MARK: - [PRIVATE] - Helper Method(s)

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* cc::easy::OAuth2HTTPClient::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}

/**
 * @brief Ensure a valud HTTP reply.
 *
 * @param a_object Test subject.
 */
const ::ev::curl::Reply* cc::easy::OAuth2HTTPClient::EnsureReply (::ev::Object* a_object)
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
