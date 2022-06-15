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

#include <string>
#include <map>
#include <mutex>
#include <inttypes.h>

#include "osal/datagram_socket.h"

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            class Producer : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            public: // Data Type(s)
                
                enum class Status : uint8_t
                {
                    Pending,
                    Busy,
                    Failed
                };
                            
                typedef struct {
                    const std::string& query_;       //!< PostgreSQL query.
                    const void*        client_ptr_;  //!< Pointer to client.
                } Order;

                typedef struct {
                    std::string uuid_;   //!< Universal Unique ID.
                    uint64_t    index_;  //!< Index in pending queue, only valid for NON failure status.
                    uint64_t    total_;  //!< Number of orders in queue.
                    Status      status_; //!< Order status.
                    std::string reason_; //!< Only set on failure status.
                } Ticket;

            private: // Threading
                
                std::mutex mutex_;

            private: // Data Type(s)
                
                typedef struct {
                    std::string query_;       //!< PostgreSQL query.
                    const void* client_ptr_;  //!< Pointer to client.
                } Pending;
                
            private: // Data
                
                std::map<std::string, Pending> pending_; //!< Pending orders, must be under mutex umbrella.
            
            private: // Networking
                
                osal::DatagramServerSocket    socket_;
                
            public: // Constructor(s) / Destructor
                
                Producer();
                virtual ~Producer();
            
            public: // Method(s) / Function(s) - One Shot Call Only!
                
                virtual void Start () = 0;
                virtual void Stop  () = 0;
                
            public: // Method(s) / Function(s)
                
                Ticket Queue  (const Order& a_order);
                void   Cancel (const Ticket& a_ticket);
                
            private: // Method(s) / Function(s)
                
                void NotifyConsumer (const std::string& a_uuid, const Order& a_order);

            }; // end of class 'Producer'

        } // end of namespace 'offloader'

    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_PRODUCER_H_
