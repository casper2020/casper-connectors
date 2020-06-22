/**
* @file types.h
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
* along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cc/debug/types.h"

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::debug::ThreadingOneShotInitializer::ThreadingOneShotInitializer (cc::debug::Threading& a_instance)
    : ::cc::Initializer<cc::debug::Threading>(a_instance)
{
    instance_.main_thread_id_ = cc::debug::Threading::k_invalid_thread_id_;
}

/**
 * @brief Destructor.
 */
cc::debug::ThreadingOneShotInitializer::~ThreadingOneShotInitializer ()
{
    /* empty */
}

const cc::debug::Threading::ThreadID cc::debug::Threading::k_invalid_thread_id_ = 0;

#ifdef __APPLE__
#pragma mark -
#endif
