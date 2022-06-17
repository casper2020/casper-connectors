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
    shared_ptr_ = nullptr;
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
}

// MARK: -

/**
 * @brief Start producer.
 *
 * @param a_shared_ptr Pointer to shared data.
 */
void cc::postgresql::offloader::Producer::Start (::cc::postgresql::offloader::Shared* a_shared_ptr)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... keep track of shared data ...
    if ( nullptr != shared_ptr_ ) {
        shared_ptr_->Reset();
    }
    shared_ptr_ = a_shared_ptr;
}

/**
 * @brief Stop producer.
 */
void cc::postgresql::offloader::Producer::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... keep track of shared data ...
    if ( nullptr != shared_ptr_ ) {
        shared_ptr_->Reset();
        shared_ptr_ = nullptr;
    }
}

// MARK: -

/**
 * @brief Queue an query execution order.
 *
 * @param a_order Execution order, see \link offloader::Order \link..
 *
 * @return Execution ticket, see \link offloader::Ticket \link.
 */
cc::postgresql::offloader::Ticket
cc::postgresql::offloader::Producer::Queue (const cc::postgresql::offloader::Order& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr != shared_ptr_);
    // ... queue order ...
    return shared_ptr_->Queue(a_order);
}

/**
 * @brief Try to cancel a query execution.
 *
 * @param a_ticket Execution ticket, see \link offloader::Ticket \link.
 */
void cc::postgresql::offloader::Producer::Cancel (const cc::postgresql::offloader::Ticket& a_ticket)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr != shared_ptr_);
    // ... cancel tickt ...
    shared_ptr_->Cancel(a_ticket);
}


