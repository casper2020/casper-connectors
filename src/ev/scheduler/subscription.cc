/**
 * @file subscription.cc
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

#include "ev/scheduler/subscription.h"

const char* const ev::scheduler::Subscription::k_status_strings_[] = {
  "NotSet",
  "Subscribing",
  "Subscribed",
  "Unsubscribing",
  "Unsubscribed"
};

/**
 * @brief Default constructor.
 *
 * @param a_callback
 */
ev::scheduler::Subscription::Subscription (EV_SUBSCRIPTION_COMMIT_CALLBACK a_commit_callback)
    : ev::scheduler::Object(ev::scheduler::Object::Type::Subscription)
{
    commit_callback_ = a_commit_callback;
}

/**
 * @brief Destructor.
 */
ev::scheduler::Subscription::~Subscription ()
{
    commit_callback_ = nullptr;
}
