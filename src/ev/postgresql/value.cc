/**
 * @file value.cc - PostgreSQL Value
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

#include "ev/postgresql/value.h"


ev::postgresql::Value ev::postgresql::Value::k_null_;

/**
 * @brief Default constructor.
 */
ev::postgresql::Value::Value ()
    : ev::Object(ev::Object::Type::Value, ev::Object::Target::PostgreSQL),
      content_type_(ev::postgresql::Value::ContentType::Null)
{
    pg_result_     = nullptr;
    error_status_  = PGRES_COMMAND_OK;
    error_message_ = nullptr;
}

/**
 * @brief Destructor.
 */
ev::postgresql::Value::~Value ()
{
    if ( nullptr != pg_result_ ) {
        PQclear(pg_result_);
    }
    if ( nullptr != error_message_ ) {
        delete [] error_message_;
    }
}
