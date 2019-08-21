/**
 * @file types.h
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
#ifndef NRS_EV_CONFIG_H_
#define NRS_EV_CONFIG_H_

#include "ev/object.h"

#include "cc/types.h"

#include <string> // std::string
#include <set>    // std::set
#include <map>    // std::map

#include <stdlib.h> // random

namespace ev
{
    typedef struct _DeviceLimits {
        
        size_t                   max_conn_per_worker_;
        ssize_t                  max_queries_per_conn_;
        ssize_t                  min_queries_per_conn_;
        
        inline void operator=(const _DeviceLimits& a_limits)
        {
            max_conn_per_worker_  = a_limits.max_conn_per_worker_;
            max_queries_per_conn_ = a_limits.max_queries_per_conn_;
            min_queries_per_conn_ = a_limits.min_queries_per_conn_;
        }
        
        static ssize_t s_rnd_queries_per_conn_ (const _DeviceLimits& a_limits)
        {
            ssize_t max_queries_per_conn = -1;
            if ( a_limits.min_queries_per_conn_ > -1 && a_limits.max_queries_per_conn_ > -1 ) {
                // ... both limits are set ...
                max_queries_per_conn = a_limits.min_queries_per_conn_ +
                (
                 random() % ( a_limits.max_queries_per_conn_ - a_limits.min_queries_per_conn_ + 1 )
                );
            } else if ( -1 == a_limits.min_queries_per_conn_ && a_limits.max_queries_per_conn_ > -1  ) {
                // ... only upper limit is set ...
                max_queries_per_conn = a_limits.max_queries_per_conn_;
            } // else { /* ignored */ }
            return max_queries_per_conn;
        };        
        
    } DeviceLimits;

    typedef std::map<ev::Object::Target, ev::DeviceLimits> DeviceLimitsMap;

    typedef ::cc::Directories Directories;
    
} // end of namespace 'ev'

#endif // NRS_EV_CONFIG_H_
