/**
 * @file supervisor.cc
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

#include "cc/postgresql/offloader/supervisor.h"

// MARK: -

/**
 * @brief Default constructor.
 */
cc::postgresql::offloader::Supervisor::Supervisor ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... initialize ...
    producer_ptr_ = nullptr;
    consumer_ptr_ = nullptr;
    shared_       = nullptr;
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::Supervisor::~Supervisor ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... simulate a 'Stop' call ...
    Stop();
}

// MARK: -

/**
 * @brief Start supervisor.
 *
 * @param a_config          Configuration.
 * @param a_polling_timeout Loop polling timeout in millseconds, if < 0 will use defaults.
 */
void cc::postgresql::offloader::Supervisor::Start (const Supervisor::Config& a_config, const float& a_polling_timeout)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr == shared_);
    // ... setup ...
    const auto pair = Setup();
    CC_ASSERT(nullptr != pair.first && nullptr != pair.second);
    // ... ....
    shared_ = new Shared(a_config);
    // ... start all helpers ...
    producer_ptr_ = pair.first;
    producer_ptr_->Start(shared_);
    consumer_ptr_ = pair.second;
    consumer_ptr_->Start(shared_, a_polling_timeout);
}

/**
 * @brief Stop supervisor.
 */
void cc::postgresql::offloader::Supervisor::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... stop all helpers ...
    if ( nullptr != producer_ptr_ ) {
        producer_ptr_->Stop();
    }
    if ( nullptr != consumer_ptr_ ) {
        consumer_ptr_->Stop();
    }
    // ... clean up ...
    const bool dismantle = ( nullptr != producer_ptr_ || nullptr != consumer_ptr_ );
    if ( true == dismantle ) {
        Dismantle(std::make_pair(producer_ptr_, consumer_ptr_));
        producer_ptr_ = nullptr;
        consumer_ptr_ = nullptr;
    }
    if ( nullptr != shared_ ) {
        delete shared_;
        shared_ = nullptr;
    }
    clients_.clear();
}

// MARK: -

/**
 * @brief Asynchronously exceute a query.
 * 
 * @param a_query  PostgreSQL query to perform.
 * @param a_client Client that wants to order a query load.
 *
 * @return One of \link Supervisor::Status \link.
 */
cc::postgresql::offloader::Supervisor::Status
cc::postgresql::offloader::Supervisor::Queue (cc::postgresql::offloader::Client* a_client, const std::string& a_query)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... issue an order an keep track of it's ticket ...
    const auto ticket = producer_ptr_->Queue(offloader::Order{
       /* query_       */ a_query,
        /* client_ptr_ */ a_client
    });
    // ... if succeded ...
    // ... keep track of this client <-> ticket ...
    Track(a_client, ticket);
    if ( offloader::Status::Pending != ticket.status_ ) {
        Untrack(a_client);
    }
    // ... done ...
    return ticket.status_;
}

/**
 * @brief Issue a cancellation order for all pending 'load' order for a specific client.
 *
 * @param a_client Client that wants to cancel all previous orders.
 */
void cc::postgresql::offloader::Supervisor::Cancel (cc::postgresql::offloader::Client* a_client)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... check of client is being tracked ...
    const auto it = clients_.find(a_client);
    if ( clients_.end() == it ) {
        // ... client not tracked ...
        return;
    }
    // ... cancel all tickets for the provided client ...
    for ( const auto& ticket : it->second ) {
        producer_ptr_->Cancel(ticket);
    }
    // ... forget client ...
    Untrack(a_client);
}

