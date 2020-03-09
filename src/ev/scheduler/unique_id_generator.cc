/**
 * @file unique_id_generator.cc
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

#include "ev/scheduler/unique_id_generator.h"

#include "osal/osalite.h"

#include "ev/exception.h"
#include <limits> // std::numeric_limits

uint64_t             ev::scheduler::UniqueIDGenerator::k_invalid_id_ = 0;
uint64_t             ev::scheduler::UniqueIDGenerator::next_         = ev::scheduler::UniqueIDGenerator::k_invalid_id_;
std::set<uint64_t>   ev::scheduler::UniqueIDGenerator::rented_;
std::queue<uint64_t> ev::scheduler::UniqueIDGenerator::cached_;
void*                ev::scheduler::UniqueIDGenerator::last_owner_   = nullptr;

/**
 * @brief 'Rent' an unique id.
 *
 * @param a_owner
 */
uint64_t ev::scheduler::UniqueIDGenerator::Rent (void* /* a_owner */)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    uint64_t rv;
//    if ( cached_.size() > 0 ) {
//        rv = cached_.front();
//        cached_.pop();
//    } else {
//        if ( ( next_ + 1 ) == std::numeric_limits<uint64_t>::max() ) {
//            throw ev::Exception("Out of unique IDs while generating a new id!");
//        }
//        rv = ++next_;
//    }
//    // ... avoid sequential ids ...
//    if ( a_owner == last_owner_ ) {
//        if ( ( next_ + 1 ) == std::numeric_limits<uint64_t>::max() ) {
//            throw ev::Exception("Out of unique IDs while avoiding sequential id!");
//        }
        rv = ++next_;
//    }
//    last_owner_ = a_owner;
    return rv;
}

/**
 * @brief 'Return' an unique id.
 *
 * @param a_id Previously rented id.
 * @param a_owner
 */
void ev::scheduler::UniqueIDGenerator::Return (void* /* a_owner */, uint64_t /* a_id */)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
//    const auto it = rented_.find(a_id);
//    if ( rented_.end() != it ) {
//        rented_.erase(it);
//        cached_.push(a_id);
//    }
}

/**
 * @brief Initialize data.
 */
void ev::scheduler::UniqueIDGenerator::Startup ()
{
    while ( cached_.size() > 0 ) {
        cached_.pop();
    }
    rented_.clear();
    next_ = ev::scheduler::UniqueIDGenerator::k_invalid_id_;
}

/**
 * @brief Reset all data.
 */
void ev::scheduler::UniqueIDGenerator::Shutdown ()
{
    while ( cached_.size() > 0 ) {
        cached_.pop();
    }
    rented_.clear();
    next_ = 0;
}
