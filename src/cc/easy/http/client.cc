/**
 * @file client.cc
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/easy/http/client.h"

#include "cc/macros.h"

#include "cc/b64.h"

#include "ev/curl/http.h" // for static helpers

#include "cc/easy/json.h"

#include <map>

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data TO BE COPIED
 * @param a_user_agent    User-Agent header value.
 */
cc::easy::http::Client::Client (const ev::Loggable::Data& a_loggable_data, const char* const a_user_agent)
 : cc::easy::http::Base(a_loggable_data, a_user_agent)
{
    Enable(this);
}

/**
 * @brief Destructor.
 */
cc::easy::http::Client::~Client ()
{
    /* empty */
}

// MARK: - [PRIVATE] - Helper Method(s)

/**
 * @brief Perform an HTTP request.
 *
 * @param a_method   Method, one of \link http::Client::Method \link.
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 *
 * @param a_timeouts See \link http:Client::Timeouts \link.
 */
void cc::easy::http::Client::Async (const http::Client::Method a_method,
                                    const std::string& a_url, const http::Client::Headers& a_headers, const std::string* a_body,
                                    http::Client::Callbacks a_callbacks, const http::Client::Timeouts* a_timeouts)
{
    Base::Async(new ::ev::curl::Request(loggable_data_, a_method, a_url, &a_headers, a_body, a_timeouts), {}, a_callbacks);
}
