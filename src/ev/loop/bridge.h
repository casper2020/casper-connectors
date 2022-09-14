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
#ifndef NRS_EV_LOOP_BRIDGE_H_
#define NRS_EV_LOOP_BRIDGE_H_

#include "ev/bridge.h"

#include "osal/datagram_socket.h"
#include "osal/condition_variable.h"

#include <thread>  // std::thread
#include <atomic>
#include <functional>

#include <event2/event.h> // libevent2

#include "cc/debug/types.h"

namespace ev
{

    namespace loop
    {

        class Bridge final : public ::ev::Bridge
        {
            
        public: // Data Type(s)
            
            typedef std::function<void(const ev::Exception& a_ev_exception)>     FatalExceptionCallback;
            typedef std::function<void(const std::function<void()>& a_callback)> CallOnMainThreadCallback;
            
        private: // Data Type(s)
            
            class Callback
            {
            public: // Data
                
                std::chrono::steady_clock::time_point start_time_point_;
                struct event*                         event_;
                uint64_t                               timeout_ms_;
                void*                                 parent_ptr_;
                
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
                Callback (std::function<void()> a_function, uint64_t a_timeout_ms)
                {
                    start_time_point_    = std::chrono::steady_clock::now();
                    no_payload_function_ = std::move(a_function);
                    payload_             = nullptr;
                    event_               = nullptr;
                    timeout_ms_          = a_timeout_ms;
                    parent_ptr_          = nullptr;
                }
                
                /**
                 * @brief Constructor for callback w/payload.
                 *
                 * @param a_function
                 * @param a_payload
                 * @param a_timeout_ms
                 */
                Callback (std::function<void(void* a_payload)> a_function, void* a_payload, uint64_t a_timeout_ms)
                {
                    start_time_point_ = std::chrono::steady_clock::now();
                    payload_function_ = std::move(a_function);
                    payload_          = a_payload;
                    event_            = nullptr;
                    timeout_ms_       = a_timeout_ms;
                    parent_ptr_       = nullptr;
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~Callback ()
                {
                    payload_function_    = nullptr;
                    no_payload_function_ = nullptr;
                    if ( nullptr != event_ ) {
                        event_del(event_);
                        event_free(event_);
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
            
        protected: // Data
            
            std::string                  name_;
            
        protected: // Theading

#if 0 // TODO
            osal::ConditionVariable      detach_condition_;
#endif
            osal::ConditionVariable      abort_condition_;
#if 0 // TODO
            std::thread*                 thread_;
#endif
            CC_IF_DEBUG_DECLARE_VAR(cc::debug::Threading::ThreadID, thread_id_;)
            
            std::atomic<bool>              aborted_;
            std::atomic<bool>              running_;
            
        protected: // Event
            
            struct event_base*         event_base_;
            struct event*              hack_event_;
            struct event*              watchdog_event_;
            struct event*              socket_event_;
            
            std::atomic<int>           pending_callbacks_count_;
            
        protected: // Data
            
            uint8_t*                   rx_buffer_;
            size_t                     rx_buffer_length_;
            size_t                     rx_buffer_bytes_count_;
            
        protected: // Bridge
            
            osal::DatagramServerSocket socket_;
            
        private: // Callbacks
            
            FatalExceptionCallback 	 fatal_exception_callback_;
            CallOnMainThreadCallback call_on_main_thread_;
            
        public: // Constructor(s) / Destructor
            
            Bridge ();
            virtual ~Bridge ();
            
        public: // Method(s) / Function(s)
            
            CallOnMainThreadCallback Start (const std::string& a_name,
                                            const std::string& a_socket_fn,
                                            FatalExceptionCallback a_fatal_exception_callback);
            void Stop  (int a_sig_no);
            void Quit  ();
            
            bool IsRunning () const;
            
        public: // Inherited Virtual Method(s) / Function(s) - from ::ev::Bridge
            
            virtual void    CallOnMainThread    (std::function<void(void* a_payload)> a_callback, void* a_payload, uint64_t a_timeout_ms = 0);
            virtual void    CallOnMainThread    (std::function<void()> a_callback, uint64_t a_timeout_ms);
            virtual void    CallOnMainThread    (std::function<void()> a_callback);
            virtual void    ThrowFatalException (const ev::Exception& a_ev_exception);
            
        // TODO private: // Virtual Method(s) / Function(s)
        public:

            void Loop                           (const bool a_at_main_thread);
            void ScheduleCalbackOnMainThread    (Callback* a_callback, uint64_t a_timeout_ms);

        private:

            void ScheduleCalbackOnThisThread    (Callback* a_callback, int64_t a_timeout_ms);
            
        private: // Method(s) / Function(s)
            
            static void EventFatalCallback    (int a_error);
            static void EventLogCallback      (int a_severity, const char* a_msg);

            static void SocketCallback        (evutil_socket_t a_fd, short a_flags, void* a_arg);

            static void LoopHackEventCallback (evutil_socket_t a_fd, short a_what, void* a_arg);
            static void WatchdogCallback      (evutil_socket_t a_fd, short a_flags, void* a_arg);
            
            static void DeferredScheduleCallback  (evutil_socket_t /* a_fd */, short a_flags, void* a_arg);
            
        }; // end of class 'bridge'
        
        inline bool Bridge::IsRunning () const
        {
            return running_;
        }

    } // end of namespace 'loop'

} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BRIDGE_H_
