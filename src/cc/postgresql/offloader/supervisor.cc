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
 * @param a_name   Parent process name or abbreviation to be used for thread name prefix.
 * @param a_config Configuration.
 */
void cc::postgresql::offloader::Supervisor::Start (const std::string& a_name, const Supervisor::Config& a_config)
{
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "~> %s()", __FUNCTION__);
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
    consumer_ptr_->Start(a_name,
                         /* a_listener */ {
                            /* on_performed_ */ std::bind(&Supervisor::OnOrderFulfilled, this, std::placeholders::_1),
                            /* on_failure_   */ std::bind(&Supervisor::OnOrderFailed   , this, std::placeholders::_1, std::placeholders::_2),
                            /* on_cancelled_ */ std::bind(&Supervisor::OnOrderCancelled, this, std::placeholders::_1)
                         },
                         shared_);
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "<~ %s", __FUNCTION__);
}

/**
 * @brief Stop supervisor.
 */
void cc::postgresql::offloader::Supervisor::Stop ()
{
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "~> %s()", __FUNCTION__);
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
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "<~ %s", __FUNCTION__);
}

// MARK: -

/**
 * @brief Asynchronously exceute a query.
 * 
 * @param a_query            PostgreSQL query to perform.
 * @param a_client           Client that wants to order a query load.
 * @param a_sucess_callback  Function to call on sucess.
 * @param a_failure_callback Function to call on failure.
 *
 * @return One of \link Supervisor::Status \link.
 */
cc::postgresql::offloader::Supervisor::Status
cc::postgresql::offloader::Supervisor::Queue (cc::postgresql::offloader::Client* a_client, const std::string& a_query,
                                              cc::postgresql::offloader::SuccessCallback a_success_callback, cc::postgresql::offloader::FailureCallback a_failure_callback)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "~> %s(%p,\"%s\")", __FUNCTION__, a_client, a_query.c_str());
    // ... issue an order an keep track of it's ticket ...
    const auto ticket = producer_ptr_->Queue(offloader::Order{
       /* query_       */ a_query,
        /* client_ptr_ */ a_client,
        /* on_success_ */ a_success_callback,
        /* on_failure  */ a_failure_callback
    });
    // ... if succeded ...
    // ... keep track of this client <-> ticket ...
    Track(a_client, ticket);
    if ( offloader::Status::Pending != ticket.status_ ) {
        Untrack(a_client);
    }
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "<~ %s(%p) - %s - " UINT8_FMT, __FUNCTION__, a_client, ticket.uuid_.c_str(), ticket.status_);
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
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "~> %s(%p)", __FUNCTION__, a_client);
    // ... check of client is being tracked ...
    const auto it = clients_.find(a_client);
    if ( clients_.end() == it ) {
        // ... for debug purposes only ...
        CC_DEBUG_LOG_MSG("offloader::Supervisor", "<~ %s(%p)", __FUNCTION__, a_client);
        // ... client not tracked ...
        return;
    }
    // ... cancel all tickets for the provided client ...
    for ( const auto& ticket : it->second ) {
        consumer_ptr_->Cancel(ticket);
    }
    // ... forget client ...
    Untrack(a_client);
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Supervisor", "<~ %s(%p)", __FUNCTION__, a_client);
}

// MARK: -

/**
 * @brief Notify producer that an order was fulfilled.
 *
 * @param a_result Order result data.
 */
void cc::postgresql::offloader::Supervisor::OnOrderFulfilled (const ::cc::postgresql::offloader::OrderResult& a_result)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... sanity check ...
    CC_DEBUG_ASSERT(nullptr != a_result.table_ || nullptr != a_result.exception_ );
    // ... if it was being tracked ...
    if ( true == Untrack(const_cast<cc::postgresql::offloader::Client*>(a_result.client_ptr_), a_result.uuid_) ) {
        // ... deliver ..
        if ( nullptr != a_result.exception_ ) {
            a_result.on_failure_(a_result.query_, *a_result.exception_);
        } else {  /* if ( nullptr != a_result.table_ ) { */
            a_result.on_success_(a_result.query_, *a_result.table_, a_result.elapsed_);
        }
    }
}

/**
 * @brief Notify producer that an order failed.
 *
 * @param a_result    Order result data.
 * @param a_exception Exception occurred.
 */
void cc::postgresql::offloader::Supervisor::OnOrderFailed (const ::cc::postgresql::offloader::OrderResult& a_result,
                                                           const ::cc::Exception& a_exception)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... if it was being tracked ...
    if ( true == Untrack(const_cast<cc::postgresql::offloader::Client*>(a_result.client_ptr_), a_result.uuid_) ) {
        // ... deliver ..
        a_result.on_failure_(a_result.query_, a_exception);
    }
}

/**
 * @brief Notify producer that an order was cancelled.
 *
 * @param a_order Cancelled pending order info.
 */
void cc::postgresql::offloader::Supervisor::OnOrderCancelled (const ::cc::postgresql::offloader::PendingOrder& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... if it was being tracked ...
    (void)Untrack(const_cast<cc::postgresql::offloader::Client*>(a_order.client_ptr_), a_order.uuid_);
}
