/**
 * @file bridge.h
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
#ifndef NRS_EV_NGX_BRIDGE_H_
#define NRS_EV_NGX_BRIDGE_H_

#include "osal/osal_singleton.h"
#include "osal/datagram_socket.h"

#include "ev/bridge.h"

#include "ev/ngx/includes.h"

#include <functional> // std::function
#include <atomic>     // std::atomic
#include  <chrono>    // std::chrono

namespace ev
{
    
    namespace ngx
    {

        class Bridge final : public ::ev::Bridge, public osal::Singleton<Bridge>
        {
            
        private: // Data Type(s)
            
            class Callback
            {
            public: // Data
                
                std::chrono::steady_clock::time_point start_time_point_;
                ngx_event_t*                          ngx_event_;
                int64_t                               timeout_ms_;

            private:
                
                std::function<void(void* a_payload)>  payload_function_;
                std::function<void()>                 no_payload_function_;
                void*                                 payload_;

            public: // Constructor(s) / Destructor
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_function
                 * @param a_timeout_ms
                 */
                Callback (std::function<void()> a_function, int64_t a_timeout_ms)
                {
                    start_time_point_    = std::chrono::steady_clock::now();
                    no_payload_function_ = std::move(a_function);
                    payload_             = nullptr;
                    ngx_event_           = nullptr;
                    timeout_ms_          = a_timeout_ms;
                }
                
                /**
                 * @brief Constructor for callback w/payload.
                 *
                 * @param a_function
                 * @param a_payload
                 * @param a_timeout_ms
                 */
                Callback (std::function<void(void* a_payload)> a_function, void* a_payload, int64_t a_timeout_ms)
                {
                    start_time_point_ = std::chrono::steady_clock::now();
                    payload_function_ = std::move(a_function);
                    payload_          = a_payload;
                    ngx_event_        = nullptr;
                    timeout_ms_       = a_timeout_ms;
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~Callback ()
                {
                    payload_function_    = nullptr;
                    no_payload_function_ = nullptr;
                    if ( nullptr != ngx_event_ ) {
                        if ( ngx_event_->timer_set ) {
                            ngx_del_timer(ngx_event_);
                        }
                        free(ngx_event_);
                    }
                }
                
            public: // Method(s) / Function(s)
                
                inline void Call ()
                {
                    if ( nullptr != no_payload_function_ ) {
                        no_payload_function_();
                        no_payload_function_ = nullptr;
                    } else if ( nullptr != payload_function_ ) {
                        payload_function_(payload_);
                        payload_function_ = nullptr;
                    }
                }
                
            };
            
        private: // NGX Data
            
            static ngx_connection_t*          connection_;
            static ngx_event_t*               event_;
            static ngx_log_t*                 log_;
            
        private: // Data
            
            static uint8_t*                   buffer_;
            static size_t                     buffer_length_;
            static size_t                     buffer_bytes_count_;

        private: // Data
            
            std::atomic<int>                  pending_callbacks_count_;
            
            osal::DatagramServerSocket        socket_;
            
        private: // Callbacks
            
            std::function<void(const ev::Exception& a_ev_exception)> fatal_exception_callback_;
                        
        public: // One-shot Call Method(s) / Function(s)
            
            void Startup  (const std::string& a_socket_fn,
                           std::function<void(const ev::Exception& a_ev_exception)> a_fatal_exception_callback);
            void Shutdown ();
            
        public: // Inherited Virtual Method(s) / Function(s) - from ::ev::Bridge
            
            virtual void    CallOnMainThread    (std::function<void(void* a_payload)> a_callback, void* a_payload, int64_t a_timeout_ms = 0);
            virtual void    CallOnMainThread    (std::function<void()> a_callback,int64_t a_timeout_ms = 0);
            virtual void    ThrowFatalException (const ev::Exception& a_ev_exception);
            
        private:
            
            void ScheduleCalbackOnMainThread (Callback* a_callback, int64_t a_timeout_ms);
            
        private: // Static Method(s) / Function(s)
            
            static void    Handler         (ngx_event_t* a_event);
            static void    DifferedHandler (ngx_event_t* a_event);
            static ssize_t Receive         (ngx_connection_t* a_connection, u_char* a_buffer, size_t a_size);
            static ssize_t Send            (ngx_connection_t* a_connection, u_char* a_buffer, size_t a_size);
            
        }; // end of class 'Bridge'

    } // end of namespace 'ngx'
    
} // end of namespace 'ev'

#endif // NRS_EV_NGX_BRIDGE_H_
