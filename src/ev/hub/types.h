/**
 * @file types.h
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
#ifndef NRS_EV_HUB_TYPES_H_
#define NRS_EV_HUB_TYPES_H_

namespace ev
{
    
    namespace hub
    {
        
        //
        // PublishPair
        //
        
        template <typename Callback> class PublishPair
        {
            
        protected: // Refs
            
            Callback callback_;
            
        public: // Constructor(s) / Destructor
            
            PublishPair (Callback a_callback)
                : callback_(a_callback)
            {
                /* empty */
            }
            
            virtual ~PublishPair ()
            {
                /* empty */
            }
            
        public: // Method(s) / Function(s)
            
            virtual void Call (std::function<void*()> a_background, std::function<void(void* /* a_payload */, Callback /* a_callback */)> a_foreground) = 0;
            
        };
        
        //
        // NextCallback
        //
        
        typedef std::function<bool(const uint64_t a_invoke_id, const ev::Object::Target a_target, const uint8_t a_tag, ev::Result* a_result)>                NextStepCallback;
        
        class NextCallback : public PublishPair<NextStepCallback>
        {
            
        public: // Constructor(s) / Destructor
            
            NextCallback (NextStepCallback a_callback)
                : PublishPair<NextStepCallback>(a_callback)
            {
                /* empty */
            }
            
            
            virtual ~NextCallback ()
            {
                /* empty */
            }
            
        };
        
        //
        // PublishCallback
        //
        
        typedef std::function<void(const uint64_t a_invoke_id, const ev::Object::Target a_target, const uint8_t a_tag, std::vector<ev::Result*>& a_results)> PublishStepCallback;
        
        class PublishCallback : public PublishPair<PublishStepCallback>
        {
            
        public: // Constructor(s) / Destructor
            
            PublishCallback (PublishStepCallback a_callback)
                : PublishPair<PublishStepCallback>(a_callback)
            {
                /* empty */
            }
            
            virtual ~PublishCallback ()
            {
                /* empty */
            }
            
        };
        
        //
        // DisconnectedCallback
        //
        
        typedef std::function<void(const uint64_t a_invoke_id, const ev::Object::Target a_target, const uint8_t a_tag)>                                      DisconnectedStepCallback;
        
        class DisconnectedCallback : public PublishPair<DisconnectedStepCallback>
        {
            
        public: // Constructor(s) / Destructor
            
            DisconnectedCallback (DisconnectedStepCallback a_callback)
                : PublishPair<DisconnectedStepCallback>(a_callback)
            {
                /* empty */
            }
            
            
            virtual ~DisconnectedCallback ()
            {
                /* empty */
            }
            
        };
        
        //
        // StepperCallbacks
        //
        
        typedef std::function<::ev::Device*(const ::ev::Object* a_target)> DeviceFactoryStepCallback;
        typedef std::function<void(::ev::Device* a_device)>                DeviceSetupStepCallback;
        typedef std::function<size_t(const ::ev::Object::Target a_target)> DeviceLimitsStepCallback;

        
        class StepperCallbacks
        {
            
        public: // Pointers
            
            NextCallback*             next_;
            PublishCallback*          publish_;
            DisconnectedCallback*     disconnected_;
            DeviceFactoryStepCallback factory_;
            DeviceSetupStepCallback   setup_;
            DeviceLimitsStepCallback  limits_;
            
        public: // Constructor / Destructor
            
            /**
             * @brief Default constructor.
             */
            StepperCallbacks ()
            {
                next_         = nullptr;
                publish_      = nullptr;
                disconnected_ = nullptr;
                factory_      = nullptr;
                setup_        = nullptr;
                limits_       = nullptr;
            }
            
            /**
             * @brief Destructor.
             */
            virtual ~StepperCallbacks ()
            {
                /* all other pointers not managed by this object */
                factory_ = nullptr;
                setup_   = nullptr;
                limits_  = nullptr;
            }
            
        };
   
    } // end of namespace 'hub'
    
} // end of namespace 'ev'

#endif // NRS_EV_HUB_TYPES_H_
