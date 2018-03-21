/**
 * @file object.c - REDIS
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

#include "ev/redis/object.h"

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 *
 * @param a_type
 */
ev::redis::Object::Object (const ev::redis::Object::Type& a_type)
    : ev::Object(a_type, ev::Object::Target::Redis)
{
    /* empty */
}

/**
 * @brief Copy constructor.
 *
 * @param a_object
 */
ev::redis::Object::Object (const ev::redis::Object& a_object)
    : ev::Object(a_object.type_, ev::Object::Target::Redis)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::redis::Object::~Object ()
{
    /* empty */
}
