/**
 * @file subscription.h
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
#pragma once
#ifndef NRS_EV_SCHEDULER_SUBSCRIPTION_H_
#define NRS_EV_SCHEDULER_SUBSCRIPTION_H_

#include "ev/scheduler/object.h"

#include "ev/request.h"

#include <set>        // std::set
#include <string>     // std::string
#include <map>        // std::map
#include <deque>      // std::deque
#include <functional> // std::function

namespace ev
{
    
    namespace scheduler
    {

        class Subscription : public ev::scheduler::Object
        {
            
        public: // Data Type(s)
            
#ifndef EV_SUBSCRIPTION_COMMIT_CALLBACK
#define EV_SUBSCRIPTION_COMMIT_CALLBACK std::function<void(ev::scheduler::Subscription* a_subscription)>
#endif
            
            enum class Status : uint8_t
            {
                NotSet = 0,
                Subscribing,
                Subscribed,
                Unsubscribing,
                Unsubscribed
            };

	public: // Static Const Data

	    static const char* const k_status_strings_[];
            
        protected: // Data
            
            EV_SUBSCRIPTION_COMMIT_CALLBACK commit_callback_; //!< Mandatory callback, to finalize subscription setup.
            
        public: // Constructor(s) / Destructor
            
            Subscription (EV_SUBSCRIPTION_COMMIT_CALLBACK a_commit_callback);
            virtual ~Subscription ();
            
        public: //
            
            virtual void Publish (std::vector<ev::Result*>& a_results) = 0;
            
        };

    } // end of namespace 'scheduler'
    
} // end of namespace 'ev'

#endif // NRS_EV_SCHEDULER_SUBSCRIPTION_H_
