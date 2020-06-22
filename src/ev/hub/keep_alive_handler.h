/**
 * @file keep_alive_handler.h
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
#ifndef NRS_EV_HUB_KEEP_ALIVE_HANDLER_H_
#define NRS_EV_HUB_KEEP_ALIVE_HANDLER_H_

#include "ev/hub/handler.h"

#include "ev/request.h"
#include "ev/device.h"

#include <map> // std::map

namespace ev
{
    
    namespace hub
    {

        class KeepAliveHandler final : public Handler
        {
            
        public: // Constructor(s) / Destructor
            
            KeepAliveHandler(StepperCallbacks& a_stepper_callbacks, cc::debug::Threading::ThreadID a_thread_id);
            virtual ~KeepAliveHandler();
            
        private: // Data
            
            enum class Command : uint8_t
            {
                SUBSCRIBE = 0,
                UNSUBSCRIBE,
                PSUBSCRIBE,
                PUNSUBSCRIBE
            };
            
            class Entry
            {
            public: // Data
                
                Request*             request_ptr_;
                Device*              device_ptr_;
                std::vector<Result*> results_;
                
            public: // Constructor(s) / Destructor
                
                /**
                 * @brief Default constructor.
                 */
                Entry (Request* a_request, Device* a_device)
                {
                    request_ptr_ = a_request;
                    device_ptr_  = a_device;
                }
                
                /*
                 * @brief Destructor.
                 */
                virtual ~Entry ()
                {
                    // ... release uncollected results ...
                    for ( auto result : results_ ) {
                        delete result;
                    }
                }
                
            };
            
            typedef std::map<const Request*, std::vector<Entry*>*> RequestToEntryMap;
            
            RequestToEntryMap                running_requests_;
            RequestToEntryMap                disconnected_requests_;
            std::map<Device*, Request*>      device_request_map_;
            std::map<Request*, Device*>      request_device_map_;
            
        public: // Inherited Pure Virtual Method(s) / Function(s) - from ev::HubHandler
            
            virtual void Idle         ();
            virtual void Push         (Request* a_request);
            virtual void SanityCheck  ();
            
        public: // Inherited Virtual Method(s) / Function(s) from ev::Device::Listener
            
            virtual void OnConnectionStatusChanged     (const ev::Device::ConnectionStatus& a_status, ev::Device* a_device);
            virtual bool OnUnhandledDataObjectReceived (const ev::Device* a_device, const ev::Request* a_request, ev::Result* a_result);
            
        private: // Method(s) / Function(s) - Callbacks Binding
            
            void DeviceConnectionCallback (const ev::Device::ConnectionStatus& a_status, ev::Device* a_device);
            
        };
        
    } // end of namespace 'hub'
    
} // end of namespace 'ev'

#endif // NRS_EV_HUB_KEEP_ALIVE_HANDLER_H_
