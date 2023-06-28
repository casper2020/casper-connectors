/**
 * @file base.h
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
#pragma once
#ifndef NRS_CC_EASY_HTTP_BASE_H_
#define NRS_CC_EASY_HTTP_BASE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/curl/request.h"
#include "ev/curl/reply.h"
#include "ev/curl/value.h"
#include "ev/curl/error.h"

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

        namespace http
        {
            
            class Base : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            public: // Data Type(s)
                
                typedef std::function<void(const uint16_t& a_code, const std::string& a_content_type, const std::string& a_body, const size_t& a_rtt)> SuccessCallback;
                typedef std::function<void(const cc::Exception& a_exception)>                                                                          FailureCallback;

                typedef ::ev::curl::Request Request;
                typedef ::ev::curl::Value   Value;
                typedef ::ev::curl::Error   Error;
                
                typedef std::function<void(const Value&)>         OnSuccessCallback;
                typedef std::function<void(const Error&)>         OnErrorCallback;
                typedef std::function<void(const cc::Exception&)> OnFailureCallback;

                typedef struct _Callbacks {
                    OnSuccessCallback on_success_;
                    OnErrorCallback   on_error_;
                    OnFailureCallback on_failure_;
                } Callbacks;
                
                typedef ::ev::curl::Request::Headers                   Headers;
                typedef std::map<std::string, Headers>                 HeadersPerMethod;
                typedef ::ev::curl::Object::cURLHeaderMapKeyComparator HeaderMapKeyComparator;
                
                typedef ::ev::curl::Request::Timeouts                  Timeouts;
                typedef ::ev::curl::Request::HTTPRequestType           Method;
                
#ifdef CC_DEBUG_ON
                            
                typedef ::ev::curl::Request::Proxy                     Proxy;
                typedef ::ev::curl::Request::CACert                    CACert;
#endif

                typedef struct {
                    std::function<void(const Request&, const std::string&)>                    log_request_;
                    std::function<void(const Value&, const std::string&)  >                    log_response_;
                    // FOR DEBUG PURPOSES ONLY - NOT READ FOR SAME CONTEXT USAGE AS THE PREVIOUS FUNCTIONS ARE
                    CC_IF_DEBUG(std::function<void(const Request&, const uint8_t, const bool)> progress_;)
                    CC_IF_DEBUG(std::function<void(const Request&, const std::string&)>        debug_   ;)
                } cURLedCallbacks;

            protected: // Const Data
                
                const ev::Loggable::Data loggable_data_;
                const std::string        user_agent_;
                
            protected: // Helper(s)
                
                cURLedCallbacks        cURLed_callbacks_;
                bool                   should_redact_;
                bool                   follow_location_;
#ifdef CC_DEBUG_ON
                bool                   ssl_do_not_verify_peer_;
                Proxy                  proxy_;
                CACert                 ca_cert_;
#endif
                ev::scheduler::Client* scheduler_client_ptr_;

            public: // Constructor / Destructor

                Base () = delete;
                Base (const ev::Loggable::Data& a_loggable_data, const char* const a_user_agent = nullptr);
                virtual ~Base();
                
            public: // Virtual Method(s) / Function(s)

                virtual void HEAD (const std::string& a_url, const Headers& a_headers,
                                   Callbacks a_callbacks, const Timeouts* a_timeouts = nullptr);

                virtual void GET  (const std::string& a_url, const Headers& a_headers,
                                   Callbacks a_callbacks, const Timeouts* a_timeouts = nullptr);

                virtual void PUT (const std::string& a_url, const Headers& a_headers, const std::string& a_body,
                                  Callbacks a_callbacks, const Timeouts* a_timeouts = nullptr);

                virtual void POST (const std::string& a_url, const Headers& a_headers, const std::string& a_body,
                                   Callbacks a_callbacks, const Timeouts* a_timeouts = nullptr);

                virtual void PATCH (const std::string& a_url, const Headers& a_headers, const std::string& a_body,
                                    Callbacks a_callbacks, const Timeouts* a_timeouts = nullptr);

                virtual void DELETE (const std::string& a_url, const Headers& a_headers, const std::string* a_body,
                                     Callbacks a_callbacks, const Timeouts* a_timeouts = nullptr);
                
            protected: // Virtual Method(s) / Function(s)

                virtual void Async (const Method a_method,
                                    const std::string& a_url, const Headers& a_headers, const std::string* a_body,
                                    Callbacks a_callbacks, const Timeouts* a_timeouts) = 0;
                
            private: // Helper Method(s) / Function(s)
                
                ::ev::scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);

            protected: // Helper Method(s) / Function(s)

                void Async (::ev::curl::Request* a_request, const std::vector<EV_TASK_CALLBACK> a_then,
                            Callbacks a_callbacks);
                            
            protected: // Static Method(s) / Function(s)
                
                static const ::ev::Result*       EnsureResult (::ev::Object* a_object);
                static const ::ev::curl::Reply*  EnsureReply  (::ev::Object* a_object);

            public: // Static Method(s) / Function(s)
                
                static void SetURLQuery (const std::string& a_url, const std::map<std::string, std::string>& a_params, std::string& o_url);

            public: // Inline Method(s) / Function(s) - one-shot call
                
                /**
                 * @brief Set log callbacks and if sensitive data should be redacted or not.
                 *
                 * @param a_callbacks See \link cURLedCallbacks \link.
                 * @param a_redact    True if should try to redact data, false otherwise.
                 */
                inline void SetcURLedCallbacks (cURLedCallbacks a_callbacks, bool a_redact)
                {
                    cURLed_callbacks_ = a_callbacks;
                    should_redact_    = a_redact;
                }
                
                /**
                 * @return True if loggable data should be reacted, false otherwise.
                 */
                inline bool cURLedShouldRedact () const
                {
                    return should_redact_;
                }
                
                /**
                 * @brief Set to allow follow any Locaotion header.
                 */
                inline void SetFollowLocation ()
                {
                    follow_location_ = true;
                }
                
#ifdef CC_DEBUG_ON
                /**
                 * @brief Set to disable SSL peer verification.
                 */
                inline void SetSSLDoNotVerifyPeer ()
                {
                    ssl_do_not_verify_peer_ = true;
                }
                
                /**
                 * @brief Set proxy.
                 *
                 * @param a_proxy Proxy config.
                 */
                inline void SetProxy (const Proxy& a_proxy)
                {
                    proxy_ = a_proxy;
                }
                
                /**
                 * @return R/O access to proxy config.
                 */
                const Proxy& proxy () const
                {
                    return proxy_;
                }
                
                /**
                 * @brief Set CA cert..
                 *
                 * @param a_cert CA Cert info.
                 */

                inline void SetCACert (const CACert& a_cert)
                {
                    ca_cert_ = a_cert;
                }
                
                /**
                 * @return R/O access to CA Cert config.
                 */
                inline const CACert& ca_cert () const
                {
                    return ca_cert_;
                }
#endif
                
                /**
                 * @return R/O access to user agent.
                 */
                inline const std::string& user_agent () const
                {
                    return user_agent_;
                }
                
            protected: // Inline Method(s) / Function(s) - one-shot call
                
                /**
                 * @brief Enable HTTP client, by setting scheduler client.
                 *
                 * @param a_client Pointer to scheduler client, memory not managed by this class.
                 */
                inline void Enable (::ev::scheduler::Client* a_client)
                {
                    CC_DEBUG_ASSERT(nullptr == scheduler_client_ptr_ && nullptr != a_client);
                    scheduler_client_ptr_ = a_client;
                    ::ev::scheduler::Scheduler::GetInstance().Register(scheduler_client_ptr_);
                }

            }; // end of class 'Base'
            
        } // end of namespace 'http'

    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_HTTP_BASE_H_
