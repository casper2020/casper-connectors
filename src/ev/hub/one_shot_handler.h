/**
 * @file one_shot_requests_handler.h
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
#ifndef NRS_EV_ONE_SHOT_REQUESTS_HANDLER_H_
#define NRS_EV_ONE_SHOT_REQUESTS_HANDLER_H_

#include "ev/hub/handler.h"

#include "ev/request.h"
#include "ev/device.h"

#include <map>    // std::map
#include <vector> // std::vector
#include <deque>  // std::deque

namespace ev
{
    
    namespace hub
    {

        class OneShotHandler final : public Handler
        {
            
        private: // Data Type(s)
            
            typedef std::map<Object::Target, std::vector<Device*>*> DevicesMap;
            typedef std::map<Object::Target, size_t>                DevicesLimits;
            
        private: // Data
            
            std::vector<Request*>       pending_requests_;
            std::deque<Request*>        completed_requests_;
            std::deque<Request*>        rejected_requests_;
            DevicesMap                  in_use_devices_;
            DevicesMap                  cached_devices_;
            std::map<Request*, Device*> request_device_map_;
            std::map<Device*, Request*> device_request_map_;
            DevicesLimits               devices_limits_;
            std::set<Device*>           zombies_;
            
        public: // Constructor(s) / Destructor
            
            OneShotHandler(StepperCallbacks& a_stepper_callbacks, osal::ThreadHelper::ThreadID a_thread_id);
            virtual ~OneShotHandler();
            
        public: // Inherited Pure Virtual Method(s) / Function(s) - from ev::hub::Handler
            
            virtual void Idle         ();
            virtual void Push         (Request* a_request);
            virtual void SanityCheck  ();
            
        public: // Inherited Virtual Method(s) / Function(s) from ev::Device::Listener
            
            virtual void OnConnectionStatusChanged     (const ev::Device::ConnectionStatus& a_status, ev::Device* a_device);
            virtual bool OnUnhandledDataObjectReceived (const ev::Device* a_device, const ev::Request* a_request, ev::Result* a_result);
            
        private: // Method(s) / Function(s)
            
            void Push        ();
            void Publish     ();
            void Link        (Request* a_request, Device* a_device);
            void Unlink      (Request* a_request);
            void KillZombies ();
            void InvalidateDevices  (const ev::Object::Target a_target);
            void PurgeDevices       ();
            
        };

    } // end of namespace 'hub'
    
} // end of namespace 'ev'

#endif // NRS_EV_ONE_SHOT_REQUESTS_HANDLER_H_
