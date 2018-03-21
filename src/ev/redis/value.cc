/**
 * @file value.cc - REDIS Value
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

#include "ev/redis/value.h"

ev::redis::Value ev::redis::Value::k_nil_;

/**
 * @brief Default constructor.
 */
ev::redis::Value::Value ()
    : ev::Object(ev::Object::Type::Value, ev::Object::Target::Redis),
      content_type_(ev::redis::Value::ContentType::Nil)
{
    integer_value_ = 0;
}

/**
 * @brief Copy constructor.
 *
 * @param a_value
 */
ev::redis::Value::Value (const ev::redis::Value& a_value)
    : ev::Object(a_value.type_, a_value.target_)
{
    Copy(a_value);
}

/**
 * @brief 'Translate0 constructor.
 *
 * @param a_reply
 */
ev::redis::Value::Value (const struct redisReply* a_reply)
    : ev::Object(ev::Object::Type::Value, ev::Object::Target::Redis)
{
    Set(a_reply);
}

/**
 * @brief Destructor.
 */
ev::redis::Value::Value::~Value ()
{
    Reset(ev::redis::Value::ContentType::Nil);
}
