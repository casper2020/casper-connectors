/**
 * @file offloader.h
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

#include "cc/ngx/postgresql/offloader.h"

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::ngx::postgresql::Offloader::Offloader ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... initialize ...
    ngx_producer_     = nullptr;
    ngx_consumer_     = nullptr;
    allow_start_call_ = false;
}

/**
 * @brief Destructor.
 */
cc::ngx::postgresql::Offloader::~Offloader ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... clean up ...
    if ( nullptr != ngx_consumer_ ) {
        delete ngx_consumer_;
    }
    if ( nullptr != ngx_producer_ ) {
        delete ngx_producer_;
    }
}

// MARK: -

/**
 * @brief Start offloader.
 *
 * @param a_config    Configuration.
 * @param a_socket_fn Socket file URI.
 * @param a_callback  Function to call on a fatal exception.
 */
void cc::ngx::postgresql::Offloader::Startup (const Offloader::Config& a_config,
                                              const std::string& a_socket_fn, Consumer::FatalExceptionCallback a_callback)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    consumer_socket_fn_   = a_socket_fn;
    consumer_fe_callback_ = a_callback;
    allow_start_call_     = true;
    Start(a_config);
}

// MARK: -

/**
 * @brief Start offloader.
 *
 * @param a_config Configuration.
 */
void cc::ngx::postgresql::Offloader::Start (const Config& a_config)
{
    CC_ASSERT(true == allow_start_call_);
    cc::postgresql::offloader::Supervisor::Start(a_config);
}

// MARK: -

/**
 * @brief Setup this instance.
 *
 * @return Pair of pointers to new instances of producer / consumer.
 */
cc::ngx::postgresql::Offloader::Pair
cc::ngx::postgresql::Offloader::Setup ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... new instances of producer / consumer are required ...
    if ( nullptr != ngx_consumer_ ) {
        delete ngx_consumer_;
    }
    if ( nullptr != ngx_producer_ ) {
        delete ngx_producer_;
    }
    ngx_producer_ = new ::cc::ngx::postgresql::Producer();
    ngx_consumer_ = new ::cc::ngx::postgresql::Consumer(consumer_socket_fn_, consumer_fe_callback_);
    // ... done ...
    return std::make_pair(ngx_producer_, ngx_consumer_);
}

/**
 * @brief Dismantle this instance.
 *
 * @param a_pair Pointers to previously create producer / consumer pair.
 */
void cc::ngx::postgresql::Offloader::Dismantle (const cc::ngx::postgresql::Offloader::Pair& a_pair)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... sanity check ...
    CC_ASSERT(a_pair.first == ngx_producer_ && a_pair.second == ngx_consumer_);
    // ... release ...
    if ( nullptr != ngx_consumer_ ) {
        delete ngx_consumer_;
        ngx_consumer_ = nullptr;
    }
    if ( nullptr != ngx_producer_ ) {
        delete ngx_producer_;
        ngx_producer_ = nullptr;
    }
    allow_start_call_ = false;
}
