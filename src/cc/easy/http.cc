/**
 * @file http.cc
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-fs-assistant.
 *
 * casper-fs-assistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-fs-assistant  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-fs-assistant.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/easy/http.h"

#include "cc/macros.h"

// MARK: - HTTPClient

/**
 * @brief Default constructor.
 *
 * @parasm a_loggable_data TO BE COPIED
 */
cc::easy::HTTPClient::HTTPClient (const ev::Loggable::Data& a_loggable_data)
 : loggable_data_(a_loggable_data)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::easy::HTTPClient::~HTTPClient ()
{
    /* empty */
}

/**
 * @brief Perform an HTTP POST request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 */
void cc::easy::HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                 HTTPClient::POSTCallbacks a_callbacks)
{
    http_.POST(loggable_data_,
               a_url, &a_headers, &a_body,
               /* a_success_callback */ [a_callbacks] (const ::ev::curl::Value& a_value) {
                    a_callbacks.on_success_(a_value.code(), a_value.header_value("Content-Type"), a_value.body(), a_value.rtt());
               },
               /* a_failure_callback */ [a_callbacks] (const ::ev::Exception& a_ev_exception) {
                    a_callbacks.on_failure_(::cc::Exception("%s", a_ev_exception.what()));
               }
    );
}

// MARK: - OAuth2HTTPClient

/**
 * @brief Default constructor.
 *
 * @param a_config OAuth2 config reference.
 */
cc::easy::OAuth2HTTPClient::OAuth2HTTPClient (const ::ev::Loggable::Data& a_loggable_data, const cc::easy::OAuth2HTTPClient::Config& a_config)
    : http_(a_loggable_data), config_(a_config)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::easy::OAuth2HTTPClient::~OAuth2HTTPClient ()
{
    /* empty */
}

/**
 * @brief Perform an HTTP POST request.
 *
 * @param a_url      URL.
 * @param a_headers  HTTP Headers.
 * @param a_body     BODY string representation.
 * @param a_callbacks Set of callbacks to report successfull or failed execution.
 *                   If the request was perform and the server replied ( we don't care about status code ) success function is called,
 *                   otheriwse, failure function is called to report the exception - usually this means client or connectivity errors not server error.
 * @param a_tokens Current access and refresh tokens - access token will be updated if invalid and when refresh is still valid.
 */
void cc::easy::OAuth2HTTPClient::POST (const std::string& a_url, const CC_HTTP_HEADERS& a_headers, const std::string& a_body,
                                       OAuth2HTTPClient::POSTCallbacks a_callbacks, Tokens& a_tokens)
{
    
    CC_HTTP_HEADERS headers;
    for ( auto it : a_headers ) {
        for ( auto it2 : it.second ) {
            headers[it.first].push_back(it2);
        }
    }
    headers["Authorization"].push_back("Bearer " + a_tokens.access_);
    
    http_.HTTPClient::POST(a_url, headers, a_body, a_callbacks);
}

// MARK: - [PRIVATE] - Helper Method(s)

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
CC_WARNING_TODO("CFA - check if this is used?")
::ev::scheduler::Task* cc::easy::OAuth2HTTPClient::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}
