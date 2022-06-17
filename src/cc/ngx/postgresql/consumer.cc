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
    delete event_;
}

// MARK: -

/**
 * @brief Start consumer.
 *
 * @param a_shared          Shared data.
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 */
void cc::ngx::postgresql::Consumer::Start (::cc::postgresql::offloader::Shared* a_shared, const float& a_polling_timeout)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... register event ...
    event_->Register(socket_fn_, fe_callback_);
    // ... continue ...
    cc::postgresql::offloader::Consumer::Start(a_shared, a_polling_timeout);
}

/**
 * @brief Stop consumer.
 */
void cc::ngx::postgresql::Consumer::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... register event from nginx event loop ...
    event_->Unregister();
    // ... continue ...
    cc::postgresql::offloader::Consumer::Stop();
}

// MARK: -

/**
 * @brief Notify producer that a ticket is ready.
 *
 * @param a_result PostgreSQL result.
 */
void cc::ngx::postgresql::Consumer::Notify (const PGresult* a_result)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // TODO: implement
    // <n>:<action>;<n>:<uuid>;<n>:<query>
    // TODO: check message format
    // socket_.Send(SIZE_FMT ":%s" SIZET_FMT ";%s:" SIZE_FMT ":%s", sizeof(char) * 4, "push", a_uuid.length(), a_uuid.c_str(), a_order.query_.length(), a_order.query_.c_str());
}
