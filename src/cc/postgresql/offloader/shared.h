/**
 * @file shared.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_SHARED_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_SHARED_H_

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
            class Shared final : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                                
            private: // Const Data
                
                const Config                               config_;     //z! PostgreSQL access config.
                
            private: // Threading
                
                std::mutex                                 mutex_;      //<! Data acess protection.
                
            private: // Data - must be under mutex umbrella.
                
                std::deque<PendingOrder*>                  orders_;    //<! Pending orders.
                std::map<std::string, PendingOrder*>       executed_;  //<! Executed orders.
                std::set<std::string>                      cancelled_; //<! Pending cancellation orders, must be under mutex umbrella.
                std::map<std::string, OrderResult*>        results_;   //<! Results  map, must be under mutex umbrella.

            private: // Callback(s)
                
                Listener                                   listener_; //<! Listener, must be under mutex umbrella.

            public: // Constructor(s) / Destructor
                
                Shared () = delete;
                Shared (const Config& a_config);
                virtual ~Shared();

            public: // Method(s) / Function(s)
                
                void   Bind   (Listener a_listener);
                void   Reset  ();
                Ticket Queue  (const Order& a_order);
                void   Cancel (const Ticket& a_ticket);

                bool  Pull (Pending& o_pending);
                void  Pop  (const Pending& o_pending, const PGresult* a_result, const uint64_t a_elapsed);
                void  Pop  (const Pending& o_pending, const ::cc::Exception& a_exception, const uint64_t a_elapsed);
                
                void  Pop  (const std::string& a_uuid, const std::function<void(const OrderResult&)>& a_callback);
            
            private: // Method(s) / Function(s)
                
                void  Purge   (const bool a_unsafe);
                void  SafePop (const Pending& o_pending, const std::function<void(const PendingOrder&)>& a_callback);
                void  SafePop (const std::string& a_uuid, const std::function<void(const OrderResult&)>& a_callback);
                
            public: // Inline Method(s) / Function(s)
                
                /**
                 * @return R/O access to configuration.
                 */
                inline const Config& config () const { return config_; }
                
            }; // end of class 'Shared'
        
        } // end of namespace 'offloader'
    
    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_SHARED_H_
