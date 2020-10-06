/**
 * @file value.cc
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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

#include "ev/curl/value.h"

/**
 * @brief Default constructor.
 *
 * @param a_code
 * @param a_headers
 * @param a_body
 * @param a_rtt
 */
ev::curl::Value::Value (const int a_code, const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body,
                        const size_t& a_rtt)
    : curl::Object(ev::Object::Type::Value),
      rtt_(a_rtt),
      code_(a_code), body_(a_body), last_modified_(0)
{
    for ( auto m : a_headers ) {
        for ( auto p : m.second ) {
            headers_[m.first].push_back(p);
        }
    }
}

/**
 * @brief Copy constructor.
 *
 * @param a_value Object to copy.
 */
ev::curl::Value::Value (const ev::curl::Value& a_value)
: curl::Object(a_value.type_),
    rtt_(a_value.rtt_),
    code_(a_value.code_), body_(a_value.body_), last_modified_(a_value.last_modified_)
{
    for ( auto m : a_value.headers_ ) {
        for ( auto p : m.second ) {
            headers_[m.first].push_back(p);
        }
    }
}

/**
 * @brief Destructor.
 */
ev::curl::Value::~Value ()
{
    /* empty */
}
