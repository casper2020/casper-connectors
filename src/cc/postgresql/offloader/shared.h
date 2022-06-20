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

#include "cc/postgresql/offloader/client.h"

#include "json/json.h"

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            // --- //
    
            typedef struct {
                std::vector<std::string>              columns_;
                std::vector<std::vector<std::string>> data_;
            } Table;

            typedef std::function<void(const std::string&, const Table&, const uint64_t)>           SuccessCallback;
            typedef std::function<void(const std::string&, const ::cc::Exception&)> FailureCallback;
            
            typedef struct {
                const std::string& query_;         //<! PostgreSQL query.
                const Client*      client_ptr_;    //<! Pointer to client.
                SuccessCallback    on_success_;    //<! Sucess callback.
                FailureCallback    on_failure_;    //<! Failure callback.
            } Order;
        
            enum class Status : uint8_t
            {
                Pending,
                Busy,
                Failed
            };
        
            typedef struct {
                std::string uuid_;   //<! Universal Unique ID.
                uint64_t    index_;  //<! Index in pending queue, only valid for NON failure status.
                uint64_t    total_;  //<! Number of orders in queue.
                Status      status_; //<! Order status.
                std::string reason_; //<! Only set on failure status.
            } Ticket;
        
            typedef struct {
                std::string uuid_;   //<! Universal Unique ID.
                std::string query_;  //<! PostgreSQL query.
            } Pending;

            typedef struct _Config {
                const std::string  str_;
                const int          statement_timeout_;
                const Json::Value& post_connect_queries_;
                const ssize_t      min_queries_per_conn_;
                const ssize_t      max_queries_per_conn_;
                const uint64_t     idle_timeout_ms_;
                const uint64_t     polling_timeout_ms_;
                inline ssize_t rnd_max_queries () const
                {
                    ssize_t max_queries_per_conn = -1;
                    if ( min_queries_per_conn_ > -1 && max_queries_per_conn_ > -1 ) {
                        // ... both limits are set ...
                        max_queries_per_conn = min_queries_per_conn_ +
                        (
                         random() % ( max_queries_per_conn_ - min_queries_per_conn_ + 1 )
                         );
                    } else if ( -1 == min_queries_per_conn_ && max_queries_per_conn_ > -1  ) {
                        // ... only upper limit is set ...
                        max_queries_per_conn = max_queries_per_conn_;
                    } // else { /* ignored */ }
                    return max_queries_per_conn;
                }

            } Config;
        
            typedef struct {
                const std::string  uuid_;          //<! Universal Unique ID.
                const std::string  query_;         //<! PostgreSQL query.
                const Client*      client_ptr_;    //<! Pointer to client.
                Table*             table_;         //<! Query execution result.
                ::cc::Exception*   exception_;     //<! Exception.
                SuccessCallback    on_success_;    //<! Sucess callback.
                FailureCallback    on_failure_;    //<! Failure callback.
                const uint64_t     elapsed_;       //<! Query execution time.
            } OrderResult;

            typedef std::function<void(const OrderResult*)> Listener;
        
        
            // --- //
            class Shared final : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            private: // Data Type(s)
                
                typedef struct {
                    const std::string uuid_;       //<! Universal Unique ID.
                    const std::string query_;      //<! PostgreSQL query.
                    const Client*     client_ptr_; //<! Pointer to client.
                    SuccessCallback   on_success_; //<! Sucess callback.
                    FailureCallback   on_failure_; //<! Failure callback.
                } PendingOrder;
                                
            private: // Const Data
                
                const Config                               config_;
                
            private: // Threading
                
                std::mutex                                 mutex_;
                
            private: // Data
                
                std::deque<PendingOrder*>                  orders_;    //<! Pending orders, must be under mutex umbrella.
                std::map<std::string, const PendingOrder*> pending_;   //<! Pending orders map, must be under mutex umbrella.
                std::set<std::string>                      cancelled_; //<! Pending cancellation orders, must be under mutex umbrella.
                std::map<std::string, OrderResult*>        results_;   //<! Results  map, must be under mutex umbrella.

            private: // Callback(s)
                
                Listener                                   listener_; //<! Listener cancellation orders, must be under mutex umbrella.

            public: // Constructor(s) / Destructor
                
                Shared () = delete;
                Shared (const Config& a_config);
                virtual ~Shared();

            public: // Method(s) / Function(s)
                
                void   Set    (Listener a_listener);
                void   Reset  ();
                Ticket Queue  (const Order& a_order);
                void   Cancel (const Ticket& a_ticket);

                bool  Peek (Pending& o_pending);
                void  Pop  (const Pending& o_pending, const PGresult* a_result, const uint64_t a_elapsed);
                void  Pop  (const Pending& o_pending, const ::cc::Exception a_exception);
                
                void  Pop  (const std::string& a_uuid, const std::function<void(const OrderResult&)>& a_callback);
            
            private: // Method(s) / Function(s)
                
                void  Purge (const bool a_unsafe);
                void  Pop   (const Pending& o_pending, const std::function<void(const PendingOrder&)>& a_callback);
                
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
