/**
 * @file object.cc
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

#include "ev/scheduler/object.h"

#include "ev/scheduler/unique_id_generator.h"

/**
 * @brief Default constructor.
 */
ev::scheduler::Object::Object (const ev::scheduler::Object::Type a_type)
    : type_(a_type)
{
    unique_id_ = ev::scheduler::UniqueIDGenerator::k_invalid_id_;
}

/**
 * @brief Destructor.
 */
ev::scheduler::Object::~Object ()
{
    if ( ev::scheduler::UniqueIDGenerator::k_invalid_id_ != unique_id_ ) {
        ev::scheduler::UniqueIDGenerator::GetInstance().Return(unique_id_);
    }
}

/**
 * @brief Return this object unique id.
 */
uint64_t ev::scheduler::Object::UniqueID ()
{
    if ( ev::scheduler::UniqueIDGenerator::k_invalid_id_ == unique_id_ ) {
        unique_id_ = ev::scheduler::UniqueIDGenerator::GetInstance().Rent();
    }
    return unique_id_;
}
