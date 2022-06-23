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
 * @param a_queue      Shared queue.
 * @param a_socket_fn  Socket file URI.
 * @param a_callback   Function to call on a fatal exception.
 *
 */
cc::ngx::postgresql::Consumer::Consumer (::cc::postgresql::offloader::Queue& a_queue, const std::string& a_socket_fn, Consumer::FatalExceptionCallback a_callback)
 : ::cc::postgresql::offloader::Consumer(a_queue)
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
 *
 * @param a_name     Parent process name or abbreviation to be used for thread name prefix.
 * @param a_listener See \link offloader::Listener \link.
 */
void cc::ngx::postgresql::Consumer::Start (const std::string& a_name, ::cc::postgresql::offloader::Listener a_listener)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... register event ...
    event_->Register(socket_fn_, fe_callback_);
    // ... register listener ...
    queue_.Bind({
        /* on_performed_ */ std::bind(&cc::ngx::postgresql::Consumer::OnOrderFulfilled, this, std::placeholders::_1),
        /* on_performed_ */ std::bind(&cc::ngx::postgresql::Consumer::OnOrderFailed   , this, std::placeholders::_1),
        /* on_cancelled_ */ std::bind(&cc::ngx::postgresql::Consumer::OnOrderCancelled, this, std::placeholders::_1)
    });
    // ... continue ...
    cc::postgresql::offloader::Consumer::Start(a_name, a_listener);
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

/**
 * @brief Called when an order was fulfilled.
 *
 * @param a_order Executed order.
 */
void cc::ngx::postgresql::Consumer::OnOrderFulfilled (const ::cc::postgresql::offloader::PendingOrder& a_order)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ...
    const std::string uuid = a_order.uuid_;
    event_->CallOnMainThread([this, uuid]() {
        // ... sanity check ...
        CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
        // ... fetch result ...
        queue_.ReleaseExecuted(uuid, [this] (const ::cc::postgresql::offloader::PendingOrder& a_order_ref) {
            try {
                // ... notify base class ...
                ::cc::postgresql::offloader::Consumer::OnOrderFulfilled(a_order_ref);
            } catch (const ::cc::Exception& a_exception) {
                // ... notify base class ...
                ::cc::postgresql::offloader::Consumer::OnOrderFailed(a_order_ref);
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

/**
 * @brief Called when an order was executed but failed.
 *
 * @param a_order Failed order.
 */
void cc::ngx::postgresql::Consumer::OnOrderFailed (const ::cc::postgresql::offloader::PendingOrder& a_order)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ...
    const std::string uuid = a_order.uuid_;
    event_->CallOnMainThread([this, uuid]() {
        // ... sanity check ...
        CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
        // ... fetch result ...
        queue_.ReleaseFailed(uuid, [this] (const ::cc::postgresql::offloader::PendingOrder& a_order_ref) {
            try {
                // ... notify base class ...
                ::cc::postgresql::offloader::Consumer::OnOrderFailed(a_order_ref);
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

/**
 * @brief Called when an order was cancelled.
 *
 * @param a_order Cancelled order.
 */
void cc::ngx::postgresql::Consumer::OnOrderCancelled (const ::cc::postgresql::offloader::PendingOrder& a_order)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ...
    const std::string uuid = a_order.uuid_;
    event_->CallOnMainThread([this, uuid]() {
        // ... sanity check ...
        CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
        // ... fetch result ...
        queue_.ReleaseCancelled(uuid, [this, uuid] (const ::cc::postgresql::offloader::PendingOrder& a_order_ref) {
            try {
                // ... notify base class ...
                ::cc::postgresql::offloader::Consumer::OnOrderCancelled(a_order_ref);
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
