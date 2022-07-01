/**
 * @file consumer.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_CONSUMER_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_CONSUMER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/postgresql/offloader/queue.h"

#include "cc/debug/types.h"
#include "cc/debug/threading.h"

#include "osal/condition_variable.h"

#include <thread>

#include <set>
#include <queue>
#include <chrono>
#include <regex>
#include <atomic>

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            class Consumer : public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            protected: // Refs
                
                Queue&                   queue_;

            protected: // Threading
                
                CC_IF_DEBUG_DECLARE_VAR (::cc::debug::Threading::ThreadID, thread_id_;)

            private: // Threading
                
                std::string              thread_nm_;
                std::thread*             thread_;
                std::atomic<bool>        aborted_;
                std::mutex               mutex_;
                osal::ConditionVariable* start_cv_;
                
            private: // PG Data
                 
                PGconn*                               connection_; //<! Must be under mutex umbrella.
                std::string                           exec_uuid_;  //<! Must be under mutex umbrella.
                
                size_t                                reuse_count_;
                ssize_t                               max_reuse_count_;
                
                std::chrono::steady_clock::time_point idle_start_;
                
            private: // Callback(s)
                
                Listener                              listener_; //<! Listener.
                                
            private: // Static Const Data
                
                static const std::regex sk_cancelled_regex_;

            public: // Constructor(s) / Destructor
                
                Consumer () = delete;
                Consumer (Queue& a_queue);
                virtual ~Consumer ();
                
            public: // Virtual Method(s) / Function(s) - One Shot Call ONLY!
                
                virtual void Start (const std::string& a_name, Listener a_listener);
                virtual void Stop  ();
            
            public: //  Method(s) / Function(s)
                
                void Cancel (const Ticket& a_ticket);
                
            protected: // Pure Virtual Method(s) / Function(s)
                
                virtual void Loop (const float& a_polling_timeout);
                
            protected: // Pure Virtual Method(s) / Function(s)
                
                virtual void OnOrderFulfilled (const ::cc::postgresql::offloader::PendingOrder& a_order);
                virtual void OnOrderFailed    (const ::cc::postgresql::offloader::PendingOrder& a_order);
                virtual void OnOrderCancelled (const ::cc::postgresql::offloader::PendingOrder& a_order);

            private: // Method(s) / Function(s)
                
                void      Connect    ();
                void      Disconnect (const bool a_idle, const char* const a_reason);
                PGresult* Execute    (::cc::postgresql::offloader::Pending& a_order, const std::set<ExecStatusType>& a_acceptable, uint64_t& o_elapsed);

            }; // end of class 'Consumer'

        } // end of namespace 'offloader'

    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_CONSUMER_H_
