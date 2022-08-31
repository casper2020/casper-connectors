/**
 * @file queue.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_QUEUE_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_QUEUE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <string>
#include <inttypes.h>

#include <deque>
#include <map>
#include <set>

#include <mutex>

#include <libpq-fe.h> // PG*

#include "cc/postgresql/offloader/types.h"
#include "cc/postgresql/offloader/client.h"

#include "json/json.h"

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {

            // --- //
            class Queue final : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                                
            private: // Const Data
                
                const Config                          config_;        //!< PostgreSQL access config.
                
            private: // Threading
                
                std::mutex                            mutex_;         //!< Data acess protection.
                
            private: // Data - must be under mutex umbrella.
                
                std::set<std::string>                 ids_;           //!< In-use ids.
                std::deque<PendingOrder*>             orders_;        //!< Pending orders.
                std::set<std::string>                 try_to_cancel_; //!< Order to cancel, must be under mutex umbrella.
                
                std::map<std::string, PendingOrder*>  executed_;      //!< Executed orders.
                std::map<std::string, PendingOrder*>  cancelled_;     //!< Truly cancelled orders, must be under mutex umbrella.
                std::map<std::string, PendingOrder*>  failed_;        //!< Executed order ( and failed ), must be under mutex umbrella.

            private: // Callback(s)
                
                Listener                              listener_;      //!< Listener, must be under mutex umbrella.

            public: // Constructor(s) / Destructor
                
                Queue () = delete;
                Queue (const Config& a_config);
                virtual ~Queue();

            public: // Method(s) / Function(s)
                
                void   Bind   (Listener a_listener);
                void   Reset  ();
                
                Ticket Enqueue  (const Order& a_order);
                void   Cancel   (const Ticket& a_ticket);

            public: // Method(s) / Function(s)
                
                bool  Peek             (Pending& o_pending);
                
                void  DequeueExecuted  (const Pending& o_pending, const PGresult* a_result, const uint64_t a_elapsed);
                void  DequeueCancelled (const Pending& o_pending, const uint64_t a_elapsed);
                void  DequeueFailed    (const Pending& o_pending, const ::cc::Exception& a_exception, const uint64_t a_elapsed);
                
                void  ReleaseExecuted  (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback);                
                void  ReleaseCancelled (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback);
                void  ReleaseFailed    (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback);
            
            private: // Method(s) / Function(s)
                
                void  ReleaseExecutedOrder  (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback);
                void  ReleaseCancelledOrder (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback);
                void  ReleaseFailedOrder    (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback);
                
                void  ReleaseOrder          (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback,
                                             std::map<std::string, PendingOrder*>& a_map, std::mutex* a_mutex) const;
                
                bool  PurgeTryCancel        (const std::string& a_uuid, const std::vector<std::map<std::string, PendingOrder*>*>& a_maps,
                                             const bool a_notify, std::mutex* a_mutex);
                
            public: // Inline Method(s) / Function(s)
                
                /**
                 * @return R/O access to configuration.
                 */
                inline const Config& config () const { return config_; }
                
            }; // end of class 'Queue'
        
        } // end of namespace 'offloader'
    
    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_QUEUE_H_
