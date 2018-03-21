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
 * @param a_body
 */
ev::curl::Value::Value (const int a_code, const std::string& a_body)
    : ev::Object(ev::Object::Type::Value, ev::Object::Target::CURL),
    code_(a_code), body_(a_body), last_modified_(0)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::curl::Value::~Value ()
{
    /* empty */
}
