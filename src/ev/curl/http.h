/**
 * @file http.h
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
#pragma once
#ifndef NRS_EV_CURL_HTTP_H_
#define NRS_EV_CURL_HTTP_H_

#include "ev/scheduler/scheduler.h"

#include "ev/loggable.h"

#include "ev/curl/value.h"
#include "ev/curl/request.h"
#include "ev/curl/error.h"

#include <functional> // std::function
#include <string>     // std::string
#include <map>        // std::map

#include "cc/macros.h"
#include "cc/debug/types.h"

namespace ev
{

    namespace curl
    {

        class HTTP : public ::ev::scheduler::Client
        {
            
        public: // Data Type(s)
                        
            typedef struct {
                std::function<void(const ::ev::curl::Request&, const std::string&)>                    log_request_;
                std::function<void(const ::ev::curl::Value&, const std::string&)  >                    log_response_;
                // FOR DEBUG PROPOSES ONLY - NOT READ FOR SAME CONTEXT USAGE AS THE PREVIOUS FUNCTIONS ARE
                CC_IF_DEBUG(std::function<void(const ::ev::curl::Request&, const uint8_t, const bool)> progress_);
                CC_IF_DEBUG(std::function<void(const ::ev::curl::Request&, const std::string&)>        debug_   );
            } cURLedCallbacks;

        public: //

            #define EV_CURL_HTTP_SUCCESS_CALLBACK std::function<void(const ::ev::curl::Value&)>
            #define EV_CURL_HTTP_ERROR_CALLBACK   std::function<void(const ::ev::curl::Error&)>
            #define EV_CURL_HTTP_FAILURE_CALLBACK std::function<void(const ::ev::Exception&)>
            #define EV_CURL_HTTP_HEADERS          EV_CURL_HEADERS_MAP
            #define EV_CURL_HTTP_TIMEOUTS         ::ev::curl::Request::Timeouts

        public: // Constructor / Destructor

            HTTP ();
            virtual ~HTTP();
            
        private: // Data
            
            cURLedCallbacks cURLed_callbacks_;
            bool            cURLed_redact_;

        public: // Method(s) / Function(s)

            void GET    (const Loggable::Data& a_loggable_data,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);
            
            void GET    (const Loggable::Data& a_loggable_data,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                         const std::string& a_uri,
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void PUT    (const Loggable::Data& a_loggable_data,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         const std::string* a_body,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void POST   (const Loggable::Data& a_loggable_data,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         const std::string* a_body,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_ERROR_CALLBACK a_error_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,                         
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void POST   (const Loggable::Data& a_loggable_data,
                         const std::string& a_uri,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);

            void PATCH  (const Loggable::Data& a_loggable_data,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         const std::string* a_body,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);
            
            void DELETE (const Loggable::Data& a_loggable_data,
                         const std::string& a_url, const EV_CURL_HTTP_HEADERS* a_headers,
                         const std::string* a_body,
                         EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback,
                         const EV_CURL_HTTP_TIMEOUTS* a_timeouts = nullptr);
            
        public: // Inline Method(s) / Function(s)
            
            /**
             * @brief Set log callbacks and if sensitive data should be redacted or not.
             *
             * @param a_callbacks See \link cURLedCallbacks \link.
             * @param a_redact    True if should try to redact data, false otherwise.
             */
            inline void Set (cURLedCallbacks a_callbacks, bool a_redact)
            {
                cURLed_callbacks_ = a_callbacks;
                cURLed_redact_    = a_redact;
            }
            
            /**
             * @return True if loggable data should be reacted, false otherwise.
             */
            inline bool cURLedShouldRedact () const
            {
                return cURLed_redact_;
            }

        private: // Method(s) / Function(s)

            void                   Async   (::ev::curl::Request* a_request,
                                            EV_CURL_HTTP_SUCCESS_CALLBACK a_success_callback, EV_CURL_HTTP_ERROR_CALLBACK a_error_callback, EV_CURL_HTTP_FAILURE_CALLBACK a_failure_callback);
            ::ev::scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);
            
        public: // DEBUG ONLY: Helper Method(s) / Function(s)
            
#if defined(CC_DEBUG_ON)
               static void DumpRequest   (const std::string& a_token, const std::string& a_id, const ::ev::curl::Request* a_request);
               static void DumpResponse  (const std::string& a_token, const std::string& a_id, const std::string& a_method, const std::string& a_url,
                                          const ::ev::curl::Value& a_value);
               static void DumpException (const std::string& a_token, const std::string& a_id, const std::string& a_method, const std::string& a_url,
                                          const ::ev::Exception& a_exception);
#endif
            static std::string cURLRequest  (const std::string& a_id, const ::ev::curl::Request* a_request, const bool a_redact);
            static std::string cURLResponse (const std::string& a_id, const std::string& a_method,
                                             const ::ev::curl::Value& a_value, const bool a_redact);

        }; // end of class 'HTTP'

    } // end of namespace 'curl'

} // end of namespace 'ev'


#endif // NRS_EV_CURL_HTTP_H_
