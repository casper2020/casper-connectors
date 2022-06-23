/**
 * @file types.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_TYPES_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_TYPES_H_

#include <string>
#include <inttypes.h>

#include <libpq-fe.h> // PG*

#include "json/json.h"

#include "cc/postgresql/offloader/client.h"

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            // --- //
    
            typedef struct _Table {
                std::vector<std::string>              columns_;
                std::vector<std::vector<std::string>> rows_;
            } Table;

            typedef std::function<void(const std::string&, const Table&, const uint64_t)> SuccessCallback;
            typedef std::function<void(const std::string&, const ::cc::Exception&)>       FailureCallback;
            
            typedef struct _Order {
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
        
            typedef struct _Ticket {
                std::string uuid_;   //<! Universal Unique ID.
                uint64_t    index_;  //<! Index in pending queue, only valid for NON failure status.
                uint64_t    total_;  //<! Number of orders in queue.
                Status      status_; //<! Order status.
                std::string reason_; //<! Only set on failure status.
            } Ticket;
        
            typedef struct {
                const std::string uuid_;       //<! Universal Unique ID.
                const std::string query_;      //<! PostgreSQL query.
                const Client*     client_ptr_; //<! Pointer to client.
                SuccessCallback   on_success_; //<! Sucess callback.
                FailureCallback   on_failure_; //<! Failure callback.
                Table*            table_;      //<! Query execution result.
                ::cc::Exception*  exception_;  //<! Exception.
                uint64_t          elapsed_;    //<! Query execution time.
            } PendingOrder;

            typedef struct {
                std::string uuid_;      //<! Universal Unique ID.
                std::string query_;     //<! PostgreSQL query.
                bool        cancelled_; //<! True if it was cancelled during execution.
            } Pending;

            typedef struct _Config {
                const std::string  str_;
                const ssize_t      min_queries_per_conn_;
                const ssize_t      max_queries_per_conn_;
                const Json::Value& post_connect_queries_;
                const uint64_t     statement_timeout_;
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
                std::function<void(const PendingOrder&)> on_performed_;
                std::function<void(const PendingOrder&)> on_failure_;
                std::function<void(const PendingOrder&)> on_cancelled_;
            } Listener;
        
        } // end of namespace 'offloader'
        
    } // end of namespace 'postgresql'
    
} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_TYPES_H_
