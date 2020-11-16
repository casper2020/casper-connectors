/**
 * @file handler.cc
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

#include "ev/hub/handler.h"

/**
 * @brief Default constructor.
 */
ev::hub::Handler::Handler (ev::hub::StepperCallbacks& a_stepper
                           CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(const cc::debug::Threading::ThreadID, a_thread_id))
    : CC_IF_DEBUG_CONSTRUCT_SET_VAR(thread_id_, a_thread_id,,) stepper_(a_stepper)  
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::hub::Handler::~Handler ()
{
    /* empty */
}
