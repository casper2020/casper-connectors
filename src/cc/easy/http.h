/**
 * @file http.h
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
#pragma once
#ifndef NRS_CC_EASY_HTTP_H_
#define NRS_CC_EASY_HTTP_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/curl/http.h"
#include "ev/curl/reply.h"

#include "ev/scheduler/scheduler.h"

namespace cc
{

    namespace easy
    {

        //
        // HTTPClient
        //
        class HTTPClient : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        #define CC_HTTP_HEADERS EV_CURL_HEADERS_MAP
            
        public: // Data Type(s)
            
            typedef std::function<void(const uint16_t& a_code, const std::string& a_content_type, const std::string& a_body, const size_t& a_rtt)> SuccessCallback;
            typedef std::function<void(const cc::Exception& a_exception)>                                                                          FailureCallback;

            typedef struct {
                SuccessCallback on_success_;
                FailureCallback on_failure_;
            } Callbacks;

        private: // Const Data
            
            const ev::Loggable::Data loggable_data_;
            
        private: // Helper(s)
            
            ev::curl::HTTP http_;

        public: // Constructor / Destructor

            HTTPClient () = delete;
            HTTPClient (const ev::Loggable::Data& a_loggable_data);
            virtual ~HTTPClient();
            
        public: // Method(s) / Function(s)
            
            void GET  (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, Callbacks a_callbacks);
            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body, Callbacks a_callbacks);

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
                URLs        urls_;
                Credentials credentials_;
                std::string redirect_uri_;
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
            
            typedef HTTPClient::Callbacks POSTCallbacks;
  
        private: // Const Data
            
            const ev::Loggable::Data    loggable_data_;

        private: // Const Refs
            
            const Config& config_;

        private: // Refs
            
            Tokens& tokens_;

        private: // Helper(s)
            
            HTTPClient http_;

        private: // Callbacks
            
            const NonStandardRequestInterceptor* nsi_ptr_; //!< When set it will be 'awesome'...
            
        public: // Constructor / Destructor

            OAuth2HTTPClient () = delete;            
            OAuth2HTTPClient (const ev::Loggable::Data& a_loggable_data, const Config& a_config, Tokens& a_tokens);
            virtual ~OAuth2HTTPClient();
            
        public: // Method(s) / Function(s)

            void AutorizationCodeGrant (const std::string& a_code,
                                        OAuth2HTTPClient::POSTCallbacks a_callbacks);

            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       OAuth2HTTPClient::POSTCallbacks a_callbacks);
            
        public: // Inline Method(s) / Function(s)
            
            void SetNonStandardRequestInterceptor (const NonStandardRequestInterceptor* a_interceptor);
            
        private: // Helper Method(s) / Function(s)
            
            ::ev::scheduler::Task*    NewTask     (const EV_TASK_PARAMS& a_callback);
            const ::ev::curl::Reply*  EnsureReply (::ev::Object* a_object);

        private: // DEBUG ONLY: Helper Method(s) / Function(s)
            
            CC_IF_DEBUG(
               void DumpRequest  (const char* const a_method, const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string* a_body) const;
               void DumpResponse (const char* const a_method, const std::string& a_url, const ::ev::curl::Value& a_value) const;
            );

        }; // end of class 'OAuth2HTTP'
    
        /**
         * Set non-standard request interceptor.
         *
         * @param a_interceptor See \link NonStandardRequestInterceptor \link.
         */
        inline void OAuth2HTTPClient::SetNonStandardRequestInterceptor (const NonStandardRequestInterceptor* a_interceptor)
        {
            nsi_ptr_ = a_interceptor;
        }
    
    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_HTTP_H_
