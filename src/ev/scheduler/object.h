/**
 * @file object.h
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
#ifndef NRS_EV_SCHEDULER_OBJECT_H_
#define NRS_EV_SCHEDULER_OBJECT_H_

#include <stdint.h> // uint8_t, uint64_t

#include "ev/request.h"
#include "ev/result.h"

namespace ev
{

    namespace scheduler
    {

        class Object
        {
            
        public: // Data Type(s)
            
            enum class Type : uint8_t
            {
                NotSet       = 0,
                Task         = 1,
                Subscription = 2
            };
            
        public: // Const Data
            
            const Type type_;
            
        private: // Data
            
            uint64_t unique_id_;
            
        public: // Constructor(s) / Destructor
            
            Object (const Type a_type);
            virtual ~Object ();
            
        public: // Pure Virtual Method(s) / Function(s)
            
            virtual bool Step         (ev::Object* a_object, ev::Request** o_request) = 0;
            virtual bool Disconnected ()                                              = 0;
            
        public: // Method(s) / Function(s)
            
            uint64_t UniqueID ();
            
        }; // end of class 'Object'

    } // end of namespace 'scheduler'
    
} // end of namespace 'ev'

#endif // NRS_EV_SCHEDULER_OBJECT_H_
