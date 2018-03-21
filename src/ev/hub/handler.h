/**
 * @file handler.h
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
#ifndef NRS_EV_HUB_HANDLER_H_
#define NRS_EV_HUB_HANDLER_H_

#include "ev/object.h"
#include "ev/request.h"
#include "ev/device.h"
#include "ev/exception.h"

#include "ev/hub/types.h"

#include <set>   // std::set
#include <deque> // std::deque

#include "osal/thread_helper.h"

namespace ev
{
    
    namespace hub
    {

        class Handler : public ev::Device::Listener, public ev::Device::Handler
        {
            
        protected: // Refs
            
            StepperCallbacks&            stepper_;
            
        protected: // Data
            
            std::set<ev::Object::Target> supported_target_;
            osal::ThreadHelper::ThreadID thread_id_;
            
        public: // Constructor(s) / Destructor
            
            Handler(StepperCallbacks& a_stepper, osal::ThreadHelper::ThreadID a_thread_id);
            virtual ~Handler();
            
        public: // Pure Virtual Method(s) / Function(s) - Declaration
            
            virtual void Idle         ()                   = 0;
            virtual void Push         (Request* a_request) = 0;
            virtual void SanityCheck  ()                   = 0;
            
        }; // end of class 'Handler'
        
    } // end of namespace 'hub'

} // end of namespace 'ev'

#endif // NRS_EV_HUB_HANDLER_H_
