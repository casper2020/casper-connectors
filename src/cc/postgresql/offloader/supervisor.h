/**
 * @file supervisor.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_SUPERVISOR_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_SUPERVISOR_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/exception.h"

#include "cc/postgresql/offloader/client.h"
#include "cc/postgresql/offloader/producer.h"
#include "cc/postgresql/offloader/consumer.h"

#include <map>    // std::map
#include <vector> // std::vector
#include <string> // std::string

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            class Supervisor : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            public: // Data Type(s)
                
                typedef offloader::Status               Status;
                
                typedef std::pair<Producer*, Consumer*> Pair;
                
                typedef offloader::Config               Config;
                
            private: // Data Type(s)
                
                typedef std::vector<offloader::Ticket> Tickets;
                typedef std::map<Client*, Tickets>     Clients;
                
            private: // Helper(s)
                
                Producer* producer_ptr_;
                Consumer* consumer_ptr_;
                
            private: // Data
                
                Queue*   shared_;
                Clients  clients_;
                    
            public: // Constructor(s) / Destructor
                
                Supervisor();
                virtual ~Supervisor();
                
            public: // Method(s) / Function(s) - One Shot Call Only
                
                virtual void Start (const std::string& a_name, const Config& a_config);
                virtual void Stop  (const bool a_destructor = false);
                
            public: // Method(s) / Function(s)
                
                Status Queue  (Client* a_client, const std::string& a_query,
                               SuccessCallback a_success_callback, FailureCallback a_failure_callback);
                void   Cancel (Client* a_client);
                
            protected: // PureInherited Virtual Method(s) / Function(s)
                
                virtual Pair Setup     (offloader::Queue& a_queue) = 0;
                virtual void Dismantle (const Pair& a_pair)        = 0;
                
            private: // Inline Method(s) / Function(s)
                
                void Track   (Client* a_client, const offloader::Ticket& a_ticket);
                void Untrack (Client* a_client);
                bool Untrack (Client* a_client, const std::string& a_uuid);
                
            private: // Method(s) / Function(s)
                
                void OnOrderFulfilled (const ::cc::postgresql::offloader::PendingOrder& a_result);
                void OnOrderFailed    (const ::cc::postgresql::offloader::PendingOrder& a_result);
                void OnOrderCancelled (const ::cc::postgresql::offloader::PendingOrder& a_order);

            }; // end of class 'Supervisor'

            /**
             * @brief Track a client.
             *
             * @param a_client Pointer to the client to track.
             * @param a_ticket Ticket to track.
             */
            inline void Supervisor::Track (Client* a_client, const offloader::Ticket& a_ticket)
            {
                // ... sanity check ...
                CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                const auto it = clients_.find(a_client);
                if ( clients_.end() == it ) {
                    // ... track ...
                    clients_[a_client] = { a_ticket };
                } else {
                    it->second.push_back(a_ticket);
                }
            }
        
            /**
             * @brief Untrack a client.
             *
             * @param a_client Pointer to the client to track.
             */
            inline void Supervisor::Untrack (Client* a_client)
            {
                // ... sanity check ...
                CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                const auto it = clients_.find(a_client);
                if ( clients_.end() != it ) {
                    clients_.erase(it);
                }
            }
            
            /**
             * @brief Untrack a client's ticket.
             *
             * @param a_client Pointer to the client to track.
             * @param a_ticket Ticket to untrack.
             *
             * @return True if it was being tracked, false otherwise.
             */
            inline bool Supervisor::Untrack (Client* a_client, const std::string& a_uuid)
            {
                // ... sanity check ...
                CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                const auto it = clients_.find(a_client);
                if ( clients_.end() == it ) {
                    // ... client not found, done ...
                    return false;
                }
                // ... find ticket ...
                for ( size_t idx = 0 ;idx < it->second.size() ; ++idx ) {
                    const auto& ticket = it->second[idx];
                    if ( 0 == ticket.uuid_.compare(a_uuid) ) {
                        // ... forget ticket ...
                        it->second.erase(it->second.begin() + idx);
                        // ... ticket found, done ...
                        return true;
                    }
                }
                // ... ticket not found, done ...
                return false;
            }
            
        } // end of namespace 'offloader'
    
    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_SUPERVISOR_H_
