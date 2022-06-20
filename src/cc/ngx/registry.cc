/**
 * @file registry.cc
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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

#include "cc/ngx/registry.h"

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::ngx::RegistryInitializer::RegistryInitializer (cc::ngx::Registry& a_instance)
    : ::cc::Initializer<::cc::ngx::Registry>(a_instance)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::ngx::RegistryInitializer::~RegistryInitializer ()
{
    std::lock_guard<std::mutex> lock(instance_.mutex_);
    instance_.events_.clear();
}

// MARK: -

/**
 * @brief Register an event.
 *
 * @param a_event Event to register.
 * @param a_data  Data related to event.
 */
void cc::ngx::Registry::Register (const ngx_event_t* a_event, const void* a_data)
{
    std::lock_guard<std::mutex> lock(mutex_);
    events_[a_event] = a_data;
}

/**
 * @brief Unregister an event.
 *
 * @param a_event Event to unregister.
 */
void cc::ngx::Registry::Unregister (const ngx_event_t* a_event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = events_.find(a_event);
    if ( events_.end() != it ) {
        events_.erase(it);
    }
}

/**
 * @brief Retrieve previously stored data for a specific event.
 *
 * @param a_event Event to check
 *
 * @return Event data, nullptr if event is not registered.
 */
const void* cc::ngx::Registry::Data (const ngx_event_t* a_event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = events_.find(a_event);
    if ( events_.end() != it ) {
        return it->second;
    }
    return nullptr;
}
