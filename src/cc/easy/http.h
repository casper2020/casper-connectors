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
            
            typedef std::function<void(const uint16_t& a_code, const std::string& a_content_type, const std::string& a_body, const size_t& a_rtt)> POSTSuccessCallback;
            typedef std::function<void(const cc::Exception& a_exception)>                                                                          POSTFailureCallback;

            typedef struct {
                POSTSuccessCallback on_success_;
                POSTFailureCallback on_failure_;
            } POSTCallbacks;

        private: // Const Data
            
            const ev::Loggable::Data loggable_data_;
            
        private: // Helper(s)
            
            ev::curl::HTTP http_;

        public: // Constructor / Destructor

            HTTPClient () = delete;
            HTTPClient (const ev::Loggable::Data& a_loggable_data);
            virtual ~HTTPClient();
            
        public: // Method(s) / Function(s)
            
            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body, POSTCallbacks a_callbacks);

        }; // end of class 'HTTP'
    
        //
        // OAuth2HTTPClient
        //
        class OAuth2HTTPClient final : public ::cc::NonCopyable, public ::cc::NonMovable,  private ::ev::scheduler::Client
        {
            
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
            } OAuth2;
            
            typedef struct {
                OAuth2 oauth2_;
            } Config;
            
            typedef struct _Tokens {
                std::string           access_;    //!< Access token value.
                std::string           refresh_;   //!< Refresh token value.
                std::function<void()> on_change_; //!< Function to call when values changes.
            } Tokens;
            
            typedef HTTPClient::POSTCallbacks POSTCallbacks;
            
        private: // Const Data
            
            const ev::Loggable::Data loggable_data_;

        private: // Const Refs
            
            const Config& config_;

        private: // Refs
            
            Tokens& tokens_;

        private: // Helper(s)
            
            HTTPClient http_;
            
        public: // Constructor / Destructor

            OAuth2HTTPClient () = delete;            
            OAuth2HTTPClient (const ev::Loggable::Data& a_loggable_data, const Config& a_config, Tokens& a_tokens);
            virtual ~OAuth2HTTPClient();
            
        public: // Method(s) / Function(s)
            
            void POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                       OAuth2HTTPClient::POSTCallbacks a_callbacks);
            
        private: // Helper Method(s) / Function(s)
            
            ::ev::scheduler::Task*    NewTask     (const EV_TASK_PARAMS& a_callback);
            const ::ev::curl::Reply*  EnsureReply (::ev::Object* a_object);

        }; // end of class 'OAuth2HTTP'
    
    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_HTTP_H_
