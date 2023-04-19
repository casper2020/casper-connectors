/**
 * @file producer.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_PRODUCER_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_PRODUCER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/exception.h"

#include "cc/postgresql/offloader/queue.h"

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            class Producer : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                                
            private: // Refs
                
                Queue& queue_;
                
            public: // Constructor(s) / Destructor
                
                Producer () = delete;
                Producer (Queue& a_queue);
                virtual ~Producer();
            
            public: // Method(s) / Function(s) - One Shot Call Only!
                
                virtual void Start ();
                virtual void Stop  ();
                
            public: // Method(s) / Function(s)
                
                Ticket Enqueue (const Order& a_order);

            }; // end of class 'Producer'

        } // end of namespace 'offloader'

    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_PRODUCER_H_
