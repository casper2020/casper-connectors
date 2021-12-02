/**
 * @file http.h
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
#pragma once
#ifndef NRS_CC_EASY_HTTP_H_
#define NRS_CC_EASY_HTTP_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/curl/http.h"
#include "ev/curl/reply.h"

#include "ev/scheduler/scheduler.h"

#include <regex>   // std::regex

#include "cc/codes.h"

#define CC_EASY_HTTP_OK                    CC_STATUS_CODE_OK
#define CC_EASY_HTTP_MOVED_TEMPORARILY     CC_STATUS_CODE_MOVED_TEMPORARILY
#define CC_EASY_HTTP_BAD_REQUEST           CC_STATUS_CODE_BAD_REQUEST
#define CC_EASY_HTTP_UNAUTHORIZED          CC_STATUS_CODE_UNAUTHORIZED
#define CC_EASY_HTTP_NOT_FOUND             CC_STATUS_CODE_NOT_FOUND
#define CC_EASY_HTTP_INTERNAL_SERVER_ERROR CC_STATUS_CODE_INTERNAL_SERVER_ERROR
#define CC_EASY_HTTP_GATEWAY_TIMEOUT       CC_STATUS_CODE_GATEWAY_TIMEOUT

namespace cc
{

    namespace easy
    {

        //
        // HTTPClient
        //
        class HTTPClient : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        #define CC_HTTP_HEADERS  EV_CURL_HEADERS_MAP
        #define CC_HTTP_TIMEOUTS EV_CURL_HTTP_TIMEOUTS
            
        public: // Data Type(s)
            
            typedef std::function<void(const uint16_t& a_code, const std::string& a_content_type, const std::string& a_body, const size_t& a_rtt)> SuccessCallback;
            typedef std::function<void(const cc::Exception& a_exception)>                                                                          FailureCallback;

            typedef struct {
                SuccessCallback on_success_;
                FailureCallback on_failure_;
            } Callbacks;

            typedef ::ev::curl::Value                         RawValue;
            typedef ::ev::curl::Error                         RawError;
            typedef std::function<void(const RawValue&)>      RawSuccessCallback;
            typedef std::function<void(const RawError&)>      RawErrorCallback;
            typedef std::function<void(const cc::Exception&)> RawFailureCallback;

            typedef struct {
                RawSuccessCallback on_success_;
                RawErrorCallback   on_error_;
                RawFailureCallback on_failure_;
            } RawCallbacks;
            
            typedef ::ev::curl::Request::Timeouts     Timeouts;
            typedef ::ev::curl::HTTP::cURLedCallbacks cURLedCallbacks;

        private: // Const Data
            
            const ev::Loggable::Data loggable_data_;
            
        private: // Helper(s)
            
            ev::curl::HTTP                  http_;
            ev::curl::HTTP::cURLedCallbacks cURLed_callbacks_;

        public: // Constructor / Destructor

            HTTPClient () = delete;
            HTTPClient (const ev::Loggable::Data& a_loggable_data);
            virtual ~HTTPClient();
            
        public: // Method(s) / Function(s)
            
            void GET  (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                       Callbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);
            
            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       Callbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void GET  (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                       RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       RawCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);
            
        public: // Inline Method(s) / Function(s) - one-shot call
            
            /**
             * @brief Set log callbacks and if sensitive data should be redacted or not.
             *
             * @param a_callbacks See \link cURLedCallbacks \link.
             * @param a_redact    True if should try to redact data, false otherwise.
             */
            inline void SetcURLedCallbacks (cURLedCallbacks a_callbacks, bool a_redact)
            {
                http_.Set(a_callbacks, a_redact);
                cURLed_callbacks_ = a_callbacks;
            }
            
            /**
             * @return True if loggable data should be reacted, false otherwise.
             */
            inline bool cURLedShouldRedact () const
            {
                return http_.cURLedShouldRedact();
            }

        }; // end of class 'HTTP'
    
        //
        // OAuth2HTTPClient
        //
        class OAuth2HTTPClient final : public ::cc::NonCopyable, public ::cc::NonMovable,  private ::ev::scheduler::Client
        {
            
        public: // Data Type(s)
            
            class NonStandardRequestInterceptor : public ::cc::NonCopyable, public ::cc::NonMovable
            {

            public: // Constructor // Destructor
                
                /**
                 * @brief Default constructor
                 */
                NonStandardRequestInterceptor ()
                {
                    /* empty */
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~NonStandardRequestInterceptor ()
                {
                    /* empty */
                }
                
            public: // Virtual Method(s) / Function(s)
                
                /**
                 * @brief Called to give a change to non-standard OAuth2 requests to deny refresh operation for a specfic request.
                 * @param a_url     URL.
                 * @param a_headers OAuth2 HTTP standard headers.
                 *
                 * @return True if refresh should continue, false it should be aborted.
                 */
                virtual bool OnUnauthorizedShouldRefresh (const std::string& /* a_url */, const CC_HTTP_HEADERS& /* a_headers */) const { return true; }
                
                /**
                 * @brief Called to give a change to non-standard OAuth2 requests ( used when server does no follow RFC or needs additional headers / data )
                 *        to set or modify non-standard data.
                 * @param a_headers OAuth2 HTTP standard headers.
                 * @param a_body    OAuth2 HTTP standard body.
                 */
                virtual void OnOAuth2RequestSet (CC_HTTP_HEADERS& /* o_headers */, std::string& /* a_body */) const {}
                
                /**
                 * @brief Called to give a change to non-standard OAuth2 requests ( used when server does no follow RFC or needs additional headers / data )
                 *        to read non-standard data.
                 *
                 * @param a_headers       HTTP standard headers.
                 * @param a_body          Response body.
                 * @param o_scope         Tokens scope.
                 * @param o_access_token  Value of the access token.
                 * @param o_refresh_token Value of the refresh token.
                 * @param o_expires_in    Expiration in seconds.
                 */
                virtual void OnOAuth2RequestReturned (const CC_HTTP_HEADERS& /* o_headers */, const std::string& /* a_body */,
                                                      std::string& /* o_scope */, std::string& /* o_access_token */, std::string& /* o_refresh_token */, size_t& /* o_expires_in */) const {}

                /**
                 * @brief Called to give a change to non-standard OAuth2 requests ( used when server does no follow RFC or needs additional headers )
                 *        to set or modify non-standard headers.
                 * @param a_headers OAuth2 HTTP standard headers.
                 */
                virtual void OnHTTPRequestHeaderSet (CC_HTTP_HEADERS& /* o_headers */) const {};
                
                
                /**
                 * @brief Called to give a process to non-standard OAuth2 requests reply ( used when server does no follow RFC or needs additional headers / data )
                 *        to check if we should consider reply a "401 - Unauthorized" response.
                 * @param a_code    HTTP status code.
                 * @param a_headers OAuth2 HTTP standard headers.
                 * @param a_body    OAuth2 HTTP standard body.
                 *
                 * @return True if we should consider a 401.
                 */
                virtual bool OnHTTPRequestReturned (const uint16_t& /* a_code */, const CC_HTTP_HEADERS& /* a_headers */, const std::string& /* a_body */) const { return false; };

            };
            
        public: // Data Type(s)
            
            typedef struct {
                std::string authorization_;
                std::string token_;
            } URLs;
            
            typedef struct {
                std::string client_id_;
                std::string client_secret_;
            } Credentials;
            
            typedef struct {
                bool        m2m_;
                URLs        urls_;
                Credentials credentials_;
                std::string redirect_uri_;
                std::string scope_;
            } OAuth2;
            
            typedef struct {
                OAuth2 oauth2_;
            } Config;
            
            typedef struct _Tokens {
                std::string           type_;       //!< Type type.
                std::string           access_;     //!< Access token value.
                std::string           refresh_;    //!< Refresh token value.
                size_t                expires_in_; //!< Number of seconds until access tokens expires.
                std::string           scope_;      //!< Service scope.
                std::function<void()> on_change_;  //!< Function to call when values changes.
            } Tokens;
            
            typedef HTTPClient::Callbacks       POSTCallbacks;
            typedef HTTPClient::RawCallbacks    RAWCallbacks;
            typedef HTTPClient::cURLedCallbacks cURLedCallbacks;
  
        private: // Const Data
            
            const ev::Loggable::Data loggable_data_;

        private: // Const Refs
            
            const Config& config_;

        private: // Refs
            
            Tokens& tokens_;

        private: // Helper(s)
            
            HTTPClient      http_;
            cURLedCallbacks cURLed_callbacks_;

        private: // Callbacks
            
            const NonStandardRequestInterceptor* nsi_ptr_; //!< When set it will be 'awesome'...
            
        public: // Constructor / Destructor

            OAuth2HTTPClient () = delete;            
            OAuth2HTTPClient (const ev::Loggable::Data& a_loggable_data, const Config& a_config, Tokens& a_tokens);
            virtual ~OAuth2HTTPClient();
            
        public: // Method(s) / Function(s)

            void AuthorizationRequest (OAuth2HTTPClient::RAWCallbacks a_callbacks);

            void AutorizationCodeGrant (const std::string& a_code,
                                        OAuth2HTTPClient::POSTCallbacks a_callbacks);

            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       OAuth2HTTPClient::POSTCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void AutorizationCodeGrant (const std::string& a_code,
                                        OAuth2HTTPClient::RAWCallbacks a_callbacks);

            void HEAD (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void GET (const std::string& a_url, const CC_HTTP_HEADERS& a_headers,
                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void DELETE (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string* a_body,
                         OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void PUT (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void PATCH (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts = nullptr);

        public: // Inline Method(s) / Function(s)
            
            void SetNonStandardRequestInterceptor (const NonStandardRequestInterceptor* a_interceptor);
            void SetcURLedCallbacks               (cURLedCallbacks a_callbacks, bool a_redact);
            
        private: // Helper Method(s) / Function(s)
            
            void SetURLQuery (const std::string& a_url, const std::map<std::string, std::string>& a_params, std::string& o_url);

            void Async   (::ev::curl::Request* a_request, const std::vector<EV_TASK_CALLBACK> a_then,
                          OAuth2HTTPClient::RAWCallbacks a_callbacks);
            
            void Async   (const ::ev::curl::Request::HTTPRequestType a_type,
                          const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string* a_body,
                          OAuth2HTTPClient::RAWCallbacks a_callbacks, const CC_HTTP_TIMEOUTS* a_timeouts);

            ::ev::scheduler::Task*    NewTask     (const EV_TASK_PARAMS& a_callback);
            const ::ev::Result*       EnsureResult (::ev::Object* a_object);
            const ::ev::curl::Reply*  EnsureReply  (::ev::Object* a_object);

        }; // end of class 'OAuth2HTTPClient'
    
        /**
         * Set non-standard request interceptor.
         *
         * @param a_interceptor See \link NonStandardRequestInterceptor \link.
         */
        inline void OAuth2HTTPClient::SetNonStandardRequestInterceptor (const NonStandardRequestInterceptor* a_interceptor)
        {
            nsi_ptr_ = a_interceptor;
        }
    
        /**
         * @brief Set log callbacks and if sensitive data should be redacted or not.
         *
         * @param a_callbacks See \link cURLedCallbacks \link.
         * @param a_redact    True if should try to redact data, false otherwise.
         */
        inline void OAuth2HTTPClient::SetcURLedCallbacks (OAuth2HTTPClient::cURLedCallbacks a_callbacks, bool a_redact)
        {
            cURLed_callbacks_ = a_callbacks;
            http_.SetcURLedCallbacks(a_callbacks, a_redact);
        }

    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_HTTP_H_
