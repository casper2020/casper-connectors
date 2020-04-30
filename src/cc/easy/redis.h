/**
 * @file redis.h
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
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef NRS_CC_EASY_REDIS_H_
#define NRS_CC_EASY_REDIS_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <stdlib.h>
#include "ev/redis/includes.h"
#include "ev/redis/reply.h"

namespace cc
{

    namespace easy
    {

        class Redis final : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        public: // Const Expr
            
            static constexpr auto k_ip_addr_  = "127.0.0.1";
            static constexpr int  k_port_nbr_ = 6379;
            
        private: // Data
            
            redisContext*      context_;
            ::ev::redis::Reply last_reply_;
            
        public: // Constructor(s) / Destructor
            
            Redis ();
            virtual ~Redis ();
            
        public: // Method(s) / Function(s)
            
            void      Connect    (const std::string& a_ip, const int a_port);
            void      Disconnect ();
            
            long long INCR       (const std::string& a_key);
            void      HSET       (const std::string& a_key, const std::string& a_field, const std::string& a_value);
            void      EXPIRE     (const std::string& a_key, const size_t& a_seconds);
            
        private: // Method(s) / Function(s)
            
            void                      EnsureConnection (const char* const a_action);
            const ::ev::redis::Reply& ExecuteCommand   (const char* const a_format, ...) __attribute__((format(printf, 2, 3)));
            
        }; // end of class 'Redis'

    } // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_REDIS_H_
