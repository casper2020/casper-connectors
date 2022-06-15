/**
 * @file consumer.cc
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

#include "cc/postgresql/offloader/consumer.h"

#include "cc/debug/types.h"

// MARK: -

/**
 * @brief Default constructor.
 */
cc::postgresql::offloader::Consumer::Consumer ()
{
    CC_IF_DEBUG_SET_VAR(thread_id_, ::cc::debug::Threading::k_invalid_thread_id_;)
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::Consumer::~Consumer ()
{
    if ( nullptr != thread_ ) {
        delete thread_;
    }
    if ( nullptr != start_cv_ ) {
        delete start_cv_;
    }
}

// MARK: -

/**
 * @brief Start consumer.
 *
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 */
void cc::postgresql::offloader::Consumer::Start (const float& a_polling_timeout)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    CC_ASSERT(nullptr == start_cv_ && nullptr == thread_);
    
    start_cv_ = new osal::ConditionVariable();
    thread_   = new std::thread(&cc::postgresql::offloader::Consumer::Loop, this, a_polling_timeout);
    thread_->detach();
    
    start_cv_->Wait();
}

/**
 * @brief Stop consumer.
 * */
void cc::postgresql::offloader::Consumer::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();

    // ... consumer thread can now be release ...
    if ( nullptr != thread_ ) {
        delete thread_;
        thread_ = nullptr;
    }
    if ( nullptr != start_cv_ ) {
        delete start_cv_;
        start_cv_ = nullptr;
    }
}

// MARK: -

/**
 * @brief Consumer loop.
 *
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 *
 */
void cc::postgresql::offloader::Consumer::Loop (const float& a_polling_timeout)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // TODO: implement
}
