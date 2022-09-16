/**
 * @file event.h
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_NGX_EVENT_H_
#define NRS_CC_NGX_EVENT_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/exception.h"

#include "ev/ngx/includes.h"

#include "osal/datagram_socket.h"

#include <functional> // std::function
#include <atomic>     // std::atomic
#include <chrono>     // std::chrono

namespace cc
{

    namespace ngx
    {
    
        class Event final : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        public: // Data Type(s)
            
            typedef std::function<void(const cc::Exception& a_ev_exception)> FatalExceptionCallback;
            
        private: // Data Type(s)
            
            class Callback
            {
                
            private: // Ptrs
                
                const Event* event_ptr_;
                
            public: // Data
                
                std::chrono::steady_clock::time_point start_time_point_;
                ngx_event_t*                          ngx_event_;
                uint64_t                              timeout_ms_;

            private:
                
                std::function<void(void* a_payload)>  payload_function_;
                std::function<void()>                 no_payload_function_;
                void*                                 payload_;

            public: // Constructor(s) / Destructor
                
                /**
                 * @brief Default constructor.
                 *
                 * @param a_event
                 * @param a_function
                 * @param a_timeout_ms
                 */
                Callback (const Event* a_event, std::function<void()> a_function, uint64_t a_timeout_ms)
                 : event_ptr_(a_event)
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
                 * @param a_event
                 * @param a_function
                 * @param a_payload
                 * @param a_timeout_ms
                 */
                Callback (const Event* a_event, std::function<void(void* a_payload)> a_function, void* a_payload, uint64_t a_timeout_ms)
                    : event_ptr_(a_event)
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
                
                inline const Event* event ()
                {
                    return event_ptr_;
                }
                
            };
            
        private: // NGX Data
            
            ngx_connection_t* connection_;
            ngx_event_t*      event_;
            ngx_log_t*        log_;
            
        private: // Data
            
            uint8_t*          buffer_;
            size_t            buffer_length_;
            size_t            buffer_bytes_count_;
            
        private: // Data
            
            std::atomic<int>        pending_callbacks_count_;
            
        private: // Callback(s)
            
            FatalExceptionCallback fatal_exception_callback_;

            
        private: // Networking
            
            osal::DatagramServerSocket socket_;

        public: // Constructor(s) / Destructor
            
            Event ();
            virtual ~Event ();
            
        public: // Method(s) / Function(s) - One Shot Call ONLY!
            
            void Register   (const std::string& a_socket_fn, FatalExceptionCallback a_callback);
            void Unregister ();
        
        public: // Method(s) / Function(s)
            
            virtual void CallOnMainThread (std::function<void(void* a_payload)> a_callback, void* a_payload, uint64_t a_timeout_ms = 0);
            virtual void CallOnMainThread (std::function<void()> a_callback, uint64_t a_timeout_ms = 0);

        private: // Method(s) / Function(s)
            
            void ScheduleCalbackOnMainThread (Callback* a_callback, uint64_t a_timeout_ms);
            void ThrowFatalException         (const cc::Exception& a_ev_exception);
            
        private: // Static Method(s) / Function(s) - Dummy
            
            static void    Handler         (ngx_event_t* a_event);
            static void    DifferedHandler (ngx_event_t* a_event);
            
            static ssize_t Receive (ngx_connection_t* a_connection, u_char* a_buffer, size_t a_size);
            static ssize_t Send    (ngx_connection_t* a_connection, u_char* a_buffer, size_t a_size);
            
        }; // end of class 'Event'
    
    } // end of namespace 'ngx'

}  // end of namespace 'cc'

#endif // NRS_CC_NGX_EVENT_H_
