/**
 * @file hub.h
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
#ifndef NRS_EV_HUB_H_
#define NRS_EV_HUB_H_

#include <thread>  // std::thread
#include <atomic>  // std::atomic
#include <chrono>  // std::chrono
#include <set>     // std::set
#include <map>     // std::map
#include <deque>   // std::deque

#include "ev/device.h"
#include "ev/request.h"
#include "ev/result.h"
#include "ev/exception.h"

#include <sys/socket.h>

#include <event.h> // libevent

#include "osal/datagram_socket.h"
#include "osal/thread_helper.h"
#include "osal/condition_variable.h"

#include "ev/bridge.h"
#include "ev/hub/one_shot_handler.h"
#include "ev/hub/keep_alive_handler.h"

namespace ev
{
    
    namespace hub
    {

        /**
         * @brief A class that defines a 'hub' for events.
         */
        class Hub
        {
            
        public: // Data Type(s)
            
            typedef std::function<void()> InitializedCallback;
            
        protected: // Data Type(s)
            
            //
            // PublishCallback
            //
            class PublishCallback : public ev::hub::PublishCallback
            {
                
            protected: // Refs
                
                ev::Bridge& bridge_;
                
            public: // Constructor(s) / Destructor
                
                PublishCallback (ev::Bridge& a_bridge, ev::hub::PublishStepCallback& a_callback);
                virtual ~PublishCallback ();
                
            public: // Method(s) / Function(s)
                
                virtual void Call (std::function<void*()> a_background,
                                   std::function<void(void* /* a_payload */, ev::hub::PublishStepCallback /* a_callback */)> a_foreground);
                
            };
            
            //
            // DisconnectedCallback
            //
            class DisconnectedCallback : public ev::hub::DisconnectedCallback
            {
                
            protected: // Refs
                
                ev::Bridge& bridge_;
                
            public: // Constructor(s) / Destructor
                
                DisconnectedCallback (ev::Bridge& a_bridge, ev::hub::DisconnectedStepCallback& a_callback);
                virtual ~DisconnectedCallback ();
                
            public: // Method(s) / Function(s)
                
                virtual void Call (std::function<void*()> a_background,
                                   std::function<void(void* /* a_payload */, ev::hub::DisconnectedStepCallback /* a_callback */)> a_foreground);
                
            };
            
            //
            // NextCallback
            //
            class NextCallback : public ev::hub::NextCallback
            {
                
            protected: // Refs
                
                ev::Bridge& bridge_;
                
            public: // Constructor(s) / Destructor
                
                NextCallback (ev::Bridge& a_bridge, ev::hub::NextStepCallback& a_callback);
                virtual ~NextCallback ();
                
            public: // Method(s) / Function(s)
                
                virtual void Call (std::function<void*()> a_background,
                                   std::function<void(void* /* a_payload */, ev::hub::NextStepCallback /* a_callback */)> a_foreground);
                
            };
            
        protected: // Refs
            
            ev::Bridge&                  bridge_;
            
        protected: // Data
            
            std::thread*                 thread_;
            std::atomic<bool>            configured_;
            std::atomic<bool>            running_;
            std::atomic<bool>            aborted_;
            
        protected: // Data
            
            struct event_base*           event_base_;
            struct event*                hack_event_;
            struct event*                watchdog_event_;
            
            std::string                  socket_file_name_;
            struct event*                socket_event_;
            uint8_t*                     socket_buffer_;
            size_t                       socket_buffer_length_;
            
            OneShotHandler*              one_shot_requests_handler_;
            KeepAliveHandler*            keep_alive_requests_handler_;
            std::set<hub::Handler*>      handlers_;
            
            std::string                  fault_msg_;
            
        private:
            
            osal::DatagramServerSocket   socket_;
            
            InitializedCallback          initialized_callback_;
            StepperCallbacks             stepper_;
            
            osal::ThreadHelper::ThreadID thread_id_;
            osal::ConditionVariable      stop_cv_;
            
            std::atomic<int>&            pending_callbacks_count_;
                        
        public: // Static Const Data
            
            static const char* const k_msg_no_payload_format_;
            static const char* const k_msg_with_payload_format_;
            static const size_t      k_msg_min_length_;
            
            static const int64_t     k_wake_msg_invalid_id_;
            
        public: // Constructor(s) / Destructor
            
            Hub (ev::Bridge& a_bridge, const std::string& a_socket_file_name, std::atomic<int>& a_pending_callbacks_count);
            virtual ~Hub ();
            
        public: // Virtual Method(s) / Function(s)
            
            virtual void Start (InitializedCallback a_initialized_callback,
                                NextStepCallback a_next_step_callback, PublishStepCallback a_publish_step_callback, DisconnectedStepCallback a_disconnected_step_callback,
                                DeviceFactoryStepCallback a_device_factory,
                                DeviceLimitsStepCallback a_device_limits_step_callback);
            virtual void Stop  (int a_sig_no);
            
        protected:
            
            virtual void Loop ();
            
        protected:
            
            void SanityCheck ();
            
        private: // STATIC - Method(s) / Function(s)
            
            static void EventFatalCallback           (int a_error);
            static void EventLogCallback             (int a_severity, const char* a_msg);
            
            static void LoopHackEventCallback        (evutil_socket_t a_fd, short a_what, void* a_arg);
            static void DatagramEventHandlerCallback (evutil_socket_t a_fd, short a_flags, void* a_arg);
            static void WatchdogCallback             (evutil_socket_t a_fd, short a_flags, void* a_arg);
            
        public:
            
            const bool IsConfigured () const;
            
        }; // end of class 'Hub'
        
        /**
         * @brief Check if this instance is configured.
         */
        inline const bool Hub::IsConfigured () const
        {
            return ( true == configured_ );
        }

    } // end of namespace 'hub'
    
} // end of namespace 'ev'

#endif // NRS_EV_HUB_H_
