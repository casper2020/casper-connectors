/**
 * @file reply.cc - PostgreSQL Reply
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

#include "ev/postgresql/reply.h"

/**
 * @brief Default constructor.
 *
 * @param a_elapsed Number of milliseconds that query execution took.
 * @param a_reply OWNERSHIP of this memory will sent value_.
 */
ev::postgresql::Reply::Reply (const uint64_t a_elapsed, PGresult* a_reply)
    : ev::postgresql::Object(ev::postgresql::Object::Type::Reply),
      elapsed_(a_elapsed)
{
    value_ = a_reply;
}

/**
 * @brief Constructor
 *
 * @param a_elapsed Number of milliseconds that query execution took.
 * @param a_status One of \link ExecStatusType \link.
 * @param a_message Repley message.
 */
ev::postgresql::Reply::Reply (const uint64_t a_elapsed, const ExecStatusType a_status, const char* const a_message)
    : ev::postgresql::Object(ev::postgresql::Object::Type::Reply),
      elapsed_(a_elapsed)
{
    value_ = { a_status, a_message };
}

/**
 * @brief Destructor.
 */
ev::postgresql::Reply::~Reply ()
{
    /* empty */
}
