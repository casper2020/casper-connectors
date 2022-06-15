/**
 * @file producer.cc
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

#include "cc/postgresql/offloader/producer.h"

#include "cc/exception.h"

#include "cc/debug/types.h"

#include <sstream>
#include <uuid/uuid.h>

// MARK: -

/**
 * @brief Default constructor.
 */
cc::postgresql::offloader::Producer::Producer ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::Producer::~Producer ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... clean up ..-
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.clear();
    
    (void)socket_.Close();
}

// MARK: -

/**
 * @brief Queue an query execution order.
 *
 * @param a_order Execution order, see \link offloader::Producer::Order \link..
 *
 * @return Execution ticket, see \link offloader::Producer::Ticket \link.
 */
cc::postgresql::offloader::Producer::Ticket
cc::postgresql::offloader::Producer::Queue (const cc::postgresql::offloader::Producer::Order& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... protecting map ...
    std::lock_guard<std::mutex> lock(mutex_);
    //
    std::string uuid, failure;
    Producer::Status  status = Producer::Status::Failed;
    try {
        // ... generate an unique ID ...
        std::stringstream ss;
        {
            char tmp[37];
            uuid_t tmp_uuid; uuid_generate_random(tmp_uuid); uuid_unparse(tmp_uuid, tmp);
            ss << tmp << std::hex << static_cast<const void*>(a_order.client_ptr_) << pending_.size();
        }
        uuid = ss.str();
        // ... avoid IDs collision ( unlikely but not impossible ) ...
        const auto it = pending_.find(uuid);
        if ( pending_.end() != it ) {
            throw ::cc::Exception("%s", "Offload request FAILED triggered by unlikely UUID collision event!");
        }
        // ... keep track if this order ...
        pending_["id"] = { a_order.query_, a_order.client_ptr_ };
        // ... send message to consumer ...
        NotifyConsumer(uuid, a_order);
        // ... accepted, mark as pending ...
        status = Producer::Status::Pending;
    } catch (const ::cc::Exception& a_cc_exception) {
        // .. FAILURE ...
        const auto it = pending_.find(uuid);
        if ( pending_.end() != it ) {
            pending_.erase(it);
        }
        failure = a_cc_exception.what();
    } catch (...) {
        // .. FAILURE ...
        const auto it = pending_.find(uuid);
        if ( pending_.end() != it ) {
            pending_.erase(it);
        }
        // ... report ...
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::Exception& a_cc_exception) {
            failure = a_cc_exception.what();
        } catch (...)  {
            failure = "???";
        }
    }
    // ... should't be possible to reach here ...
    if ( offloader::Producer::Status::Pending == status ) {
        return offloader::Producer::Ticket {
            /* uuid_       */ uuid,
            /* index_      */ static_cast<uint64_t>(pending_.size() - 1),
            /* total_      */ static_cast<uint64_t>(pending_.size()),
            /* status_     */ Producer::Status::Pending,
            /* reason      */ ""
        };
    }
    // ... FAILURE ...
    return offloader::Producer::Ticket {
        /* uuid_       */ "",
        /* index_      */ 0,
        /* total_      */ static_cast<uint64_t>(pending_.size()),
        /* status_     */ status,
        /* reason      */ failure,
    };
}

/**
 * @brief Try to cancel a query execution.
 *
 * @param a_ticket Execution ticket, see \link offloader::Producer::Ticket \link.
 */
void cc::postgresql::offloader::Producer::Cancel (const cc::postgresql::offloader::Producer::Ticket& a_ticket)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... protecting map ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... if still present, erase it ...
    const auto it = pending_.find(a_ticket.uuid_);
    if ( pending_.end() != it ) {
        pending_.erase(it);
    }
}

// MARK: -

/**
 * @brief Notify consumer that a new order is ready.
 *
 * @param a_uuid  Universal unique order identifier.
 * @param a_order Execution order, see \link offloader::Producer::Order \link..
 */
void cc::postgresql::offloader::Producer::NotifyConsumer (const std::string& a_uuid, const cc::postgresql::offloader::Producer::Order& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // TODO: implement
    // <n>:<action>;<n>:<uuid>;<n>:<query>
    // TODO: check message format
    // socket_.Send(SIZE_FMT ":%s" SIZET_FMT ";%s:" SIZE_FMT ":%s", sizeof(char) * 4, "push", a_uuid.length(), a_uuid.c_str(), a_order.query_.length(), a_order.query_.c_str());
}

