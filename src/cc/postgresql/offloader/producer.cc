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

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_queue Shared queue.
 */
cc::postgresql::offloader::Producer::Producer (offloader::Queue& a_queue)
 : queue_(a_queue)
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
}

// MARK: -

/**
 * @brief Start producer.
 */
void cc::postgresql::offloader::Producer::Start ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... keep track of shared data ...
    queue_.Reset();
}

/**
 * @brief Stop producer.
 */
void cc::postgresql::offloader::Producer::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... keep track of shared data ...
    queue_.Reset();
}

// MARK: -

/**
 * @brief Enqueue a query execution order.
 *
 * @param a_order Execution order, see \link offloader::Order \link..
 *
 * @return Execution ticket, see \link offloader::Ticket \link.
 */
cc::postgresql::offloader::Ticket
cc::postgresql::offloader::Producer::Enqueue (const cc::postgresql::offloader::Order& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... queue order ...
    return queue_.Enqueue(a_order);
}



