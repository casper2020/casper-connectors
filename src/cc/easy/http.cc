#if 0 // TODO: REMOVE THIS FILE
/**
 * @file http.cc
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
 * @param a_user_agent    User-Agent header value.
 */
cc::easy::HTTPClient::HTTPClient (const ev::Loggable::Data& a_loggable_data, const char* const a_user_agent)
 : loggable_data_(a_loggable_data)
{
    if ( nullptr != a_user_agent ) {
        http_.SetUserAgent(a_user_agent);
    }
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
 * @param a_timeouts See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::GET (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                                HTTPClient::Callbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.GET(loggable_data_,
               a_url, &a_headers,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {
                    a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
               },
              /* a_error_callback */ nullptr,
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
              a_timeouts
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
 * @param a_timeouts  See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                 HTTPClient::Callbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.POST(loggable_data_,
               a_url, &a_headers, &a_body,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {        
                    a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
               },
               /* a_error_callback */ nullptr,
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
               a_timeouts
    );
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
 * @param a_timeouts See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::HEAD (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                                HTTPClient::RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.HEAD(loggable_data_,
               a_url, &a_headers,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {
                    a_callbacks.on_success_(a_value);
               },
               /* a_error_callback */ nullptr,
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
              a_timeouts
    );
}

/**
 * @brief Perform an HTTP GET request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_timeouts See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::GET (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                                HTTPClient::RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.GET(loggable_data_,
               a_url, &a_headers,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {
                    a_callbacks.on_success_(a_value);
               },
               /* a_error_callback */ nullptr,
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
              a_timeouts
    );
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
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::HTTPClient::PUT (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                HTTPClient::RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.PUT(loggable_data_,
               a_url, &a_headers, &a_body,
               /* a_success_callback */
               [a_callbacks] (const ::ev::curl::Value& a_value) {
                    std::map<std::string, std::string> headers;
                    a_callbacks.on_success_(a_value);
               },
               /* a_error_callback */ [a_callbacks] (const ::ev::curl::Error& a_error) {
                    a_callbacks.on_error_(a_error);
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
               a_timeouts
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
 * @param a_timeouts  See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                 HTTPClient::RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.POST(loggable_data_,
               a_url, &a_headers, &a_body,
               /* a_success_callback */
               [a_callbacks] (const ::ev::curl::Value& a_value) {
                    std::map<std::string, std::string> headers;
                    a_callbacks.on_success_(a_value);
               },
               /* a_error_callback */ [a_callbacks] (const ::ev::curl::Error& a_error) {
                    a_callbacks.on_error_(a_error);
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
               a_timeouts
    );
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
 * @param a_timeouts  See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::PATCH (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                 HTTPClient::RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.PATCH(loggable_data_,
               a_url, &a_headers, &a_body,
               /* a_success_callback */
               [a_callbacks] (const ::ev::curl::Value& a_value) {
                    std::map<std::string, std::string> headers;
                    a_callbacks.on_success_(a_value);
               },
               /* a_error_callback */ [a_callbacks] (const ::ev::curl::Error& a_error) {
                    a_callbacks.on_error_(a_error);
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
               a_timeouts
    );
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
 * @param a_timeouts  See \link HTTPClient::Timeouts \link.
 */
void cc::easy::HTTPClient::DELETE (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string* a_body,
                                 HTTPClient::RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    http_.DELETE(loggable_data_,
               a_url, &a_headers, /* a_body */ nullptr,
               /* a_success_callback */
               [a_callbacks] (const ::ev::curl::Value& a_value) {
                    std::map<std::string, std::string> headers;
                    a_callbacks.on_success_(a_value);
               },
               /* a_error_callback */ [a_callbacks] (const ::ev::curl::Error& a_error) {
                    a_callbacks.on_error_(a_error);
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               },
               a_timeouts
    );
}

// MARK: - [STATIC] - Helper Method(s)

/**
 * @brief Fill an URL with a query from params map.
 *
 * @param a_url    Base URL.
 * @param a_params Parameters map.
 * @param a_url    Final URL.
 */
void cc::easy::HTTPClient::SetURLQuery (const std::string& a_url, const std::map<std::string, std::string>& a_params, std::string& o_url)
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

// MARK: - OAuth2HTTPClient

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data TO BE COPIED
 * @param a_config        R/O reference to \link OAuth2HTTPClient::Config \link.
 * @param a_tokens        R/W reference to \link OAuth2HTTPClient::Tokens \link.
 * @param a_user_agent    User-Agent header value.
 */
cc::easy::OAuth2HTTPClient::OAuth2HTTPClient (const ::ev::Loggable::Data& a_loggable_data, const OAuth2HTTPClient::Config& a_config, OAuth2HTTPClient::Tokens& a_tokens,
                                              const char* const a_user_agent)
    : loggable_data_(a_loggable_data), config_(a_config), tokens_(a_tokens), http_(loggable_data_, a_user_agent), nsi_ptr_(nullptr)
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
 * @param a_code     OAuth2 auhtorization code.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_rfc_6749 True when we should follow RFC6749, otherwise will NOT send a 'Authorization' header instead will send client_* data in body URL encoded.
 * @param a_formpost ...
 */
void cc::easy::OAuth2HTTPClient::AuthorizationCodeGrant (const std::string& a_code,
                                                         OAuth2HTTPClient::POSTCallbacks a_callbacks, const bool a_rfc_6749, const bool a_formpost)
{
    AuthorizationCodeGrant(a_code, {
        [a_callbacks] (const HTTPClient::RawValue& a_value) {
            a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
        },
        nullptr,
        a_callbacks.on_failure_
    }, a_rfc_6749, a_formpost);
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
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                       OAuth2HTTPClient::POSTCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    POST(a_url, a_headers, a_body,
         {
            [a_callbacks] (const HTTPClient::RawValue& a_value) {
                a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
            },
            nullptr,
            a_callbacks.on_failure_
        },
         a_timeouts
    );
}

/**
 * @brief Perform an HTTP POST request to obtains tokens from an 'autorization code' grant flow.
 *
 * @param a_code     OAuth2 authorization code.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 *
 * @param a_rfc_6749 True when we should follow RFC6749, otherwise will NOT send a 'Authorization' header instead will send client_* data in body URL encoded.
 * @param a_formpost ...
 */
void cc::easy::OAuth2HTTPClient::AuthorizationCodeGrant (const std::string& a_code,
                                                         OAuth2HTTPClient::RAWCallbacks a_callbacks, const bool a_rfc_6749, const bool a_formpost)
{
    AuthorizationCodeGrant(a_code, /* a_scope */ "", /* a_state */ "", a_callbacks, a_rfc_6749, a_formpost);
}

/**
 * @brief Perform an HTTP POST request to obtains tokens from an 'autorization code' grant flow.
 *
 * @param a_code     OAuth2 authorization code.
 * @param a_scope    Scope param value, empty won't be sent.
 * @param a_state    State param value, empty won't be sent.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 *
 * @param a_rfc_6749 True when we should follow RFC6749, otherwise will NOT send a 'Authorization' header instead will send client_* data in body URL encoded.s
 * @param a_formpost ...
 */
void cc::easy::OAuth2HTTPClient::AuthorizationCodeGrant (const std::string& a_code, const std::string& a_scope, const std::string& a_state,
                                                         OAuth2HTTPClient::RAWCallbacks a_callbacks, const bool a_rfc_6749, const bool a_formpost)
{
    //
    // ⚠️ 🤬 I 🤬 8 THIS CODE 🤬 ⚠️
    //
    std::string query;
    std::string url = config_.oauth2_.urls_.token_;
    CC_HTTP_HEADERS headers = { };
    
    if ( false == a_formpost ) {
        //
        // ... x-www-form-urlencoded POST ...
        //
        headers["Content-Type"] = { "application/x-www-form-urlencoded" };
        
        std::map<std::string, std::string> params = {
            { "grant_type"   , "authorization_code"          },
            { "code"         , a_code                        },
            { "redirect_uri" , config_.oauth2_.redirect_uri_ }
        };
        // ... optional params ...
        if ( 0 != a_scope.length() ) {
            params["scope"] = a_scope;
        }
        if ( 0 != a_state.length() ) {
            params["state"] = a_state;
        }
        // ... perform ...
        if ( true == a_rfc_6749 ) {
            // ... set query ...
            HTTPClient::SetURLQuery(query, params, query);
            // ... update headers ...
            headers["Authorization"] = { "Basic " + ::cc::base64_url_unpadded::encode((config_.oauth2_.credentials_.client_id_ + ':' + config_.oauth2_.credentials_.client_secret_)) };
            // ... set body ...
            const std::string body = ( a_rfc_6749 ? std::string(query.c_str() + sizeof(char)) : "" );
            // ... schedule ...
            Async(new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, config_.oauth2_.urls_.token_, &headers, &body),
                  { },
                  a_callbacks
            );
        } else {
            // ... client_* will be sent in body URL encoded ...
            params["client_id"] = config_.oauth2_.credentials_.client_id_;
            params["client_secret"] = config_.oauth2_.credentials_.client_secret_;
             // ... set query ...
            HTTPClient::SetURLQuery(query, params, query);
            // ... update url ...
            url += query;
            // ... schedule ...
            Async(new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, config_.oauth2_.urls_.token_, &headers, nullptr),
                  { },
                  a_callbacks
            );
        }
    } else {
        
        //
        // ... multipart/formdata POST ...
        //
        ::ev::curl::Request::FormFields fields = {
            { "grant_type"   , "authorization_code"          },
            { "code"         , a_code                        },
        };
        // ... perform ...
        if ( true == a_rfc_6749 ) {
            // ... update headers ...
            headers["Authorization"] = { "Basic " + ::cc::base64_url_unpadded::encode((config_.oauth2_.credentials_.client_id_ + ':' + config_.oauth2_.credentials_.client_secret_)) };
        } else {
            // ... client_* will be sent in body URL encoded ...
            fields.push_back({ "client_id"    , config_.oauth2_.credentials_.client_id_     });
            fields.push_back({ "client_secret", config_.oauth2_.credentials_.client_secret_ });
        }
        // ... optional params ...
        if ( 0 != a_scope.length() ) {
            fields.push_back({ "scope", a_scope });
        }
        if ( 0 != a_state.length() ) {
            fields.push_back({ "state", a_state });
        }
        // ...
        fields.push_back({ "redirect_uri" , config_.oauth2_.redirect_uri_ });
        // ... schedule ...
        Async(new ::ev::curl::Request(loggable_data_, config_.oauth2_.urls_.token_, &headers, fields),
              { },
              a_callbacks
        );
    }
    //  TODO: review - standard or not - if not move it from here to the proper class? + S.A.F.E implementation !?!?!?!
#if 0
    CURL *curl = curl_easy_init();
    if ( nullptr == curl ) {
        throw ::ev::Exception("Unexpected cURL handle: nullptr!");
    }

    std::string url = config_.oauth2_.urls_.token_ + "?grant_type=authorization_code";

    const std::map<std::string, std::string> map = {
        { "code"         , a_code                                      },
        { "client_id"    , config_.oauth2_.credentials_.client_id_     },
        { "client_secret", config_.oauth2_.credentials_.client_secret_ },
        { "redirect_uri" , config_.oauth2_.redirect_uri_               },
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

        if ( nullptr == a_callbacks.on_error_ ) {
            // ... no error handler ...
            const ::ev::curl::Reply* reply = EnsureReply(a_object);
            const ::ev::curl::Value& value = reply->value();
            // ... notify ...
            a_callbacks.on_success_(value);
        } else {
            const ::ev::Result*       result = EnsureResult(a_object);
            const ::ev::curl::Reply*  reply = dynamic_cast<const ::ev::curl::Reply*>(result->DataObject());
            if ( nullptr != reply ) {
                a_callbacks.on_success_(reply->value());
            } else {
                const ::ev::curl::Error* error = dynamic_cast<const ::ev::curl::Error*>(result->DataObject());
                if ( nullptr != error ) {
                    a_callbacks.on_error_(*error);
                } else {
                    throw ::ev::Exception("Unexpected CURL reply object: nullptr!");
                }
            }
        }
        
    })->Catch([a_callbacks] (const ::ev::Exception& a_ev_exception) {
        a_callbacks.on_failure_(a_ev_exception);
    });
#endif
}

/**
 * @brief Perform an HTTP POST request to obtains tokens from an 'autorization code' grant flow.
 *
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 */
void cc::easy::OAuth2HTTPClient::AuthorizationCodeGrant (OAuth2HTTPClient::RAWCallbacks a_callbacks)
{
    std::string url;
    HTTPClient::SetURLQuery(config_.oauth2_.urls_.authorization_, {
        { "response_type", "code"                                      },
        { "client_id"    , config_.oauth2_.credentials_.client_id_     },
        { "redirect_uri" , config_.oauth2_.redirect_uri_               },
        { "scope"        , config_.oauth2_.scope_                      }
    }, url);
    
    const CC_HTTP_HEADERS headers = {
        { "Content-Type" , { "application/x-www-form-urlencoded" } }
    };
    
    const std::string redirect_uri = config_.oauth2_.redirect_uri_;
    const std::string tokens_uri   = config_.oauth2_.urls_.token_;
    
    Async(new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::GET, url, &headers, nullptr),
          {
            [this, redirect_uri, tokens_uri](::ev::Object* a_object) -> ::ev::Object* {
                // ... always expecting a reply object ...
                const ::ev::curl::Reply* reply = EnsureReply(a_object);
                const ::ev::curl::Value& value = reply->value();
                // ... expected code?
                if ( 302 != value.code() ) {
                    // ... no, error not handled here ...
                    return a_object;
                }
                // ... intercept 'Location' header ...
                const std::string location = value.header_value("Location");
                if ( 0 == location.size() ) {
                    throw ::cc::Exception("Missing '%s' header: not compliant with RFC6749!", "Location");
                }
                // ... extract query ...
                std::smatch match;
                const std::regex url_expr("(.[^&?]+)?(.+)");
                if ( false == std::regex_match(location, match, url_expr) && 3 == match.size() ) {
                    throw ::cc::Exception("Invalid '%s' header value!", "Location");
                }
                // ... extract arguments ...
                const std::string args = match[2].str();
                // ... error?
                const std::regex error_expr("(\\?|&)(error=([^/&?]+))", std::regex_constants::ECMAScript | std::regex_constants::icase);
                if ( true == std::regex_match(args, match, error_expr) ) {
                    // TODO: CPW should catch coded exception
                    if ( 4 == match.size() ) {
                        throw ::cc::CodedException(404, "%s", match[3].str().c_str());
                    } else {
                        throw ::cc::Exception("Invalid '%s' header value: unable to verify if '%s' argument is present!", "Location", "error");
                    }
                }
                // ... code?
                const std::regex code_expr("(\\?|&)(code=([^/&?]+))", std::regex_constants::ECMAScript | std::regex_constants::icase);
                if ( true == std::regex_match(args, match, code_expr) ) {
                    if ( 4 == match.size() ) {
                        std::string url;
                        HTTPClient::SetURLQuery(tokens_uri, {
                            { "grant_type"   , "authorization_code"                        },
                            { "code"         , match[3].str()                              },
                        }, url);
                        const CC_HTTP_HEADERS headers = {
                            { "Authorization", { "Basic " + ::cc::base64_url_unpadded::encode((config_.oauth2_.credentials_.client_id_ + ':' + config_.oauth2_.credentials_.client_secret_)) } },
                            { "Content-Type" , { "application/x-www-form-urlencoded" } }
                        };
                        // ...perform 'access token request' ...
                        return new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::GET, url, &headers, nullptr);
                    } else {
                        throw ::cc::Exception("Invalid '%s' header value: unable to verify if '%s' argument is present!", "Location", "code");
                    }
                }
                // ... null to break the chain, if object will be used as is ...
                return a_object;
            }
        },
        a_callbacks
    );
}

/**
 * @brief Perform an HTTP POST request to obtains tokens from an 'client credentials' grant flow.
 *
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                    If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                    otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 *
 * @param a_rfc_6749 True when we should follow RFC6749, otherwise will NOT send a 'Authorization' header instead will send client_* data in body URL encoded.s
 */
void cc::easy::OAuth2HTTPClient::ClientCredentialsGrant (OAuth2HTTPClient::RAWCallbacks a_callbacks, const bool a_rfc_6749)
{
    std::string query;
    CC_HTTP_HEADERS headers = {
        { "Content-Type" , { "application/x-www-form-urlencoded" } }
    };
    if ( true == a_rfc_6749 ) {
        HTTPClient::SetURLQuery(query, {
            { "grant_type"   , "client_credentials"                        },
            { "scope"        , config_.oauth2_.scope_                      }
        }, query);
        headers["Authorization"] = { "Basic " + ::cc::base64_url_unpadded::encode((config_.oauth2_.credentials_.client_id_ + ':' + config_.oauth2_.credentials_.client_secret_)) };
    } else {
        HTTPClient::SetURLQuery(query, {
            { "grant_type"   , "client_credentials"                        },
            { "client_id"    , config_.oauth2_.credentials_.client_id_     },
            { "client_secret", config_.oauth2_.credentials_.client_secret_ },
            { "scope"        , config_.oauth2_.scope_                      }
        }, query);
    }
    const std::string body = std::string(query.c_str() + sizeof(char));
    Async(new ::ev::curl::Request(loggable_data_, ::ev::curl::Request::HTTPRequestType::POST, config_.oauth2_.urls_.token_, &headers, &body),
          { },
          a_callbacks
    );
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
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::HEAD (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
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
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::GET (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{

    Async(::ev::curl::Request::HTTPRequestType::GET, a_url, a_headers, /* a_body */ nullptr, a_callbacks, a_timeouts);
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
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::DELETE (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string* a_body,
                                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{

    Async(::ev::curl::Request::HTTPRequestType::DELETE, a_url, a_headers, /* a_body */ nullptr, a_callbacks, a_timeouts);
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
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::POST, a_url, a_headers, &a_body, a_callbacks, a_timeouts);
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
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::PUT (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::PUT, a_url, a_headers, &a_body, a_callbacks, a_timeouts);
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
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::PATCH (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    Async(::ev::curl::Request::HTTPRequestType::PATCH, a_url, a_headers, &a_body, a_callbacks, a_timeouts);
}

// MARK: - [PRIVATE] - Helper Method(s)

/**
 * @brief Perform an HTTP request.
 *
 * @param a_request  Request to execute.
 * @param a_then     Set of functions to call on sequence.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                    If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                    otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 */
void cc::easy::OAuth2HTTPClient::Async (::ev::curl::Request* a_request, const std::vector<EV_TASK_CALLBACK> a_then,
                                        OAuth2HTTPClient::RAWCallbacks a_callbacks)
{
    
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, url   , a_request->url());
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, token , CC_QUALIFIED_CLASS_NAME(this));
    const std::string id     = CC_OBJECT_HEX_ADDR(a_request);
    const std::string method = a_request->method();

    CC_IF_DEBUG(
        if ( nullptr != cURLed_callbacks_.debug_ ) {
            a_request->EnableDebug(cURLed_callbacks_.debug_);
        }
        if ( nullptr != cURLed_callbacks_.progress_ ) {
            a_request->EnableDebugProgress(cURLed_callbacks_.progress_);
        }
    )
    
    // ... prepare task ...
    ::ev::scheduler::Task* t = NewTask([CC_IF_DEBUG(token, )a_request, id, this] () -> ::ev::Object* {
        // ... log request?
        if ( nullptr != cURLed_callbacks_.log_request_ ) {
            cURLed_callbacks_.log_request_(*a_request, ::ev::curl::HTTP::cURLRequest(id, a_request, http_.cURLedShouldRedact()));
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
                cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), http_.cURLedShouldRedact()));
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
                    cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), http_.cURLedShouldRedact()));
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
                        cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), http_.cURLedShouldRedact()));
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

/**
 * @brief Perform an HTTP request.
 *
 * @param a_type     Request type, one of \link ::ev::curl::Request::HTTPRequestType \link.
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 *
 * @param a_timeouts See \link HTTPClient::ClientTimeouts \link.
 */
void cc::easy::OAuth2HTTPClient::Async (const ::ev::curl::Request::HTTPRequestType a_type,
                                        const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string* a_body,
                                        OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts)
{
    const std::string token_type = ( tokens_.type_.length() > 0 ? tokens_.type_ : "Bearer" );
    // ... copy original header ...
    CC_HTTP_HEADERS headers;
    for ( auto it : a_headers ) {
        for ( auto it2 : it.second ) {
            headers[it.first].push_back(it2);
        }
    }
    // ... apply new access token ...
    headers["Authorization"].push_back(token_type + " " + tokens_.access_);
    // ... notify interceptor?
    if ( nullptr != nsi_ptr_ ) {
        nsi_ptr_->OnHTTPRequestHeaderSet(headers);
    }
    
    const std::string tx_body = ( nullptr != a_body ? *a_body : "");
    
    // ... perform request ...
    ::ev::curl::Request* request = new ::ev::curl::Request(loggable_data_, a_type, a_url, &headers, &tx_body, a_timeouts);
    
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, url   , request->url());
    CC_IF_DEBUG_DECLARE_AND_SET_VAR(const std::string, token , CC_QUALIFIED_CLASS_NAME(this));
    const std::string id     = CC_OBJECT_HEX_ADDR(request);
    const std::string method = request->method();

    Async(new ::ev::curl::Request(loggable_data_, a_type, a_url, &headers, &tx_body, a_timeouts),
          {
            [CC_IF_DEBUG(token, )this, a_url, id, method] (::ev::Object* a_object) -> ::ev::Object* {
                
                const ::ev::curl::Reply* reply = EnsureReply(a_object);
                const ::ev::curl::Value& value = reply->value();
                
                // ... unauthorized?
                const bool unauthorized = ( 401 == value.code() || ( nullptr != nsi_ptr_ && nsi_ptr_->OnHTTPRequestReturned(value.code(), value.headers(), value.body()) ) );
                if ( true == unauthorized ) {
                    // ... log response?
                    if ( nullptr != cURLed_callbacks_.log_response_ ) {
                        cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), http_.cURLedShouldRedact()));
                    }
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
                    // ... log request?
                    if ( nullptr != cURLed_callbacks_.log_request_ ) {
                        cURLed_callbacks_.log_request_(*request, ::ev::curl::HTTP::cURLRequest(id, request, http_.cURLedShouldRedact()));
                    }
                    // ... dump request ...
                    CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, request););
                    // ... perform request ...
                    return request;
                } else {
                    // ... no, continue ..
                    return dynamic_cast<::ev::Result*>(a_object)->DetachDataObject();
                }
            },
            [CC_IF_DEBUG(token, )this, a_type, a_callbacks, a_url, a_headers, tx_body, token_type, id, method] (::ev::Object* a_object) -> ::ev::Object* {
                
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
                        // ... log response?
                        if ( nullptr != cURLed_callbacks_.log_response_ ) {
                            cURLed_callbacks_.log_response_(reply->value(), ::ev::curl::HTTP::cURLResponse(id, method, reply->value(), http_.cURLedShouldRedact()));
                        }
                        // ... dump response ...
                        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpResponse(token, id, method, a_url, reply->value()););
                        // ... keep track of new tokens ...
                        if ( nullptr != nsi_ptr_ ) {
                            nsi_ptr_->OnOAuth2RequestReturned(value.headers(), value.body(), tokens_.scope_, tokens_.access_, tokens_.refresh_, tokens_.expires_in_);
                        } else {
                            // ... expecting JSON response ...
                            const cc::easy::JSON<::ev::Exception> json;
                            Json::Value                           response;
                            json.Parse(value.body(), response);
                            // ... keep track of new tokens ...
                            tokens_.access_ = json.Get(response, "access_token" , Json::ValueType::stringValue, /* a_default */ nullptr).asString();
                            // ... optional value, no default value ...
                            if ( true == response.isMember("refresh_token") && true == response["refresh_token"].isString() ) {
                                tokens_.refresh_ = json.Get(response, "refresh_token", Json::ValueType::stringValue, /* a_default */ nullptr).asString();
                            }
                            // ... optional value, no default value ...
                            if ( true == response.isMember("token_type") && true == response["token_type"].isString() ) {
                                tokens_.type_ = response["token_type"].asString();
                            }
                            // ... optional value, no default value ...
                            if ( true == response.isMember("expires_in") && true == response["expires_in"].isNumeric() ) {
                                // ... accept it ...
                                tokens_.expires_in_ = response["expires_in"].asInt64();
                            } else {
                                // ... unknown, reset ...
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
                        headers["Authorization"].push_back(token_type + " " + tokens_.access_);
                        // ... notify interceptor?
                        if ( nullptr != nsi_ptr_ ) {
                            nsi_ptr_->OnHTTPRequestHeaderSet(headers);
                        }
                        // ... new request ...
                        ::ev::curl::Request* request = new ::ev::curl::Request(loggable_data_, a_type, a_url, &headers, &tx_body);
                        // ... log request?
                        if ( nullptr != cURLed_callbacks_.log_request_ ) {
                            cURLed_callbacks_.log_request_(*request, ::ev::curl::HTTP::cURLRequest(id, request, http_.cURLedShouldRedact()));
                        }
                        // ... dump request ...
                        CC_DEBUG_LOG_IF_REGISTERED_RUN(token, ::ev::curl::HTTP::DumpRequest(token, id, request););
                        // ... the original request must be performed again ...
                        return request;
                    } else {
                        // ... redirect to 'Finally' with OAuth2 error response ...
                        return a_object;
                    }
                }
            }
        },
        a_callbacks
    );
}

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
const ::ev::Result* cc::easy::OAuth2HTTPClient::EnsureResult (::ev::Object* a_object)
{
    ::ev::Result* result = dynamic_cast<::ev::Result*>(a_object);
    if ( nullptr == result ) {
        throw ::ev::Exception("Unexpected CURL result object: nullptr!");
    }
    return result;
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
#endif // TODO: REMOVE THIS FILE
