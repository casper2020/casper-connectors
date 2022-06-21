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

#include "cc/ngx/postgresql/consumer.h"

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_socket_fn       Socket file URI.
 * @param a_callback        Function to call on a fatal exception.
 *
 */
cc::ngx::postgresql::Consumer::Consumer (const std::string& a_socket_fn, Consumer::FatalExceptionCallback a_callback)
{
    event_       = new ::cc::ngx::Event();
    socket_fn_   = a_socket_fn;
    fe_callback_ = a_callback;
}

/**
 * @brief Destructor.
 */
cc::ngx::postgresql::Consumer::~Consumer ()
{
    // ... clean up ...
    delete event_;
}

// MARK: -

/**
 * @brief Start consumer.
 * @param a_name   Parent process name or abbreviation to be used for thread name prefix.
 * @param a_shared Shared data.
 */
void cc::ngx::postgresql::Consumer::Start (const std::string& a_name, ::cc::postgresql::offloader::Shared* a_shared)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... register event ...
    event_->Register(socket_fn_, fe_callback_);
    // ... register listener ...
    a_shared->Set(std::bind(&cc::ngx::postgresql::Consumer::Notify, this, std::placeholders::_1));
    // ... continue ...
    cc::postgresql::offloader::Consumer::Start(a_name, a_shared);
}

/**
 * @brief Stop consumer.
 */
void cc::ngx::postgresql::Consumer::Stop ()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... unregister event from nginx event loop ...
    event_->Unregister();
    // ... continue ...
    cc::postgresql::offloader::Consumer::Stop();
}

// MARK: -

/**
 * @brief Notify producer that a ticket is ready.
 *
 * @param a_result Pointer to result data.
 */
void cc::ngx::postgresql::Consumer::Notify (const ::cc::postgresql::offloader::OrderResult* a_result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ...
    const std::string uuid = a_result->uuid_;
    event_->CallOnMainThread([this, uuid]() {
        // ... sanity check ...
        CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
        // ... fetch result ...
        shared_ptr_->Pop(uuid, [] (const ::cc::postgresql::offloader::OrderResult& a_result_ref) {
            try {
                // ... sanity check ...
                CC_DEBUG_ASSERT(nullptr != a_result_ref.table_ || nullptr != a_result_ref.exception_ );
                // ... deliver ..
                if ( nullptr != a_result_ref.exception_ ) {
                    a_result_ref.on_failure_(a_result_ref.query_, *a_result_ref.exception_);
                } else {  /* if ( nullptr != a_result.table_ ) { */
                    a_result_ref.on_success_(a_result_ref.query_, *a_result_ref.table_, a_result_ref.elapsed_);
                }
            } catch (const ::cc::Exception& a_exception) {
                // ... notify ...
                a_result_ref.on_failure_(a_result_ref.query_, a_exception);
            } catch (...) {
                // ... this can not or should not happen ...
                // ... eat it or throw a fatal exception ...
                try {
                    ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
                } catch (const ::cc::Exception& a_exception) {
                    fprintf(stderr, "%s\n", a_exception.what());
                    fflush(stderr);
                }
            }
        });
    });
}
