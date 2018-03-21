/**
 * @file object.c
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

#include "ev/object.h"

/**
 * @brief Default constructor.
 */
ev::Object::Object (const ev::Object::Type a_type, const ev::Object::Target a_target)
    : type_(a_type), target_(a_target)
{
    /* empty */
}

/**
 * @brief Copy constructor.
 */
ev::Object::Object (const Object& a_object)
    : type_(a_object.type_), target_(a_object.target_)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::Object::~Object ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @return This object C string representation.
 */
const char* const ev::Object::AsCString () const
{
    return nullptr;
}
