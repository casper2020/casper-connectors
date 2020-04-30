/**
 * @file beanstalk.h
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
#ifndef NRS_CC_EASY_BEANSTALK_H_
#define NRS_CC_EASY_BEANSTALK_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"
#include "cc/types.h"

#include "ev/beanstalk/producer.h"

namespace cc
{

    namespace easy
    {

        class Beanstalk final : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        public: // Data Type
            
            enum class Mode : uint8_t {
                Producer,
                Consumer
            };
            
        public: // Const Expr
            
            static constexpr auto k_ip_addr_  = "127.0.0.1";
            static constexpr int  k_port_nbr_ = 11300;
            
        private: // Const Data
            
            const Mode mode_;
            
        private: // Helpers
            
            ::ev::beanstalk::Producer* producer_;
            
        public: // Constructor(s) / Destructor
            
            Beanstalk () = delete;
            Beanstalk (const Mode& a_mode);
            virtual ~Beanstalk();
            
        public: // Method(s) / Function(s)
            
            void Connect    (const std::string& a_ip, const int a_port, const std::set<std::string>& a_tubes, float a_timeout = 0);
            void Push       (const std::string& a_id, const size_t& a_ttr, const size_t& a_validity);
            void Push       (const std::string& a_id, const std::map<std::string, std::string>& a_args, const size_t& a_ttr, const size_t& a_validity);
            void Push       (const std::string& a_id, const std::string& a_payload, const size_t& a_ttr, const size_t& a_validity);
            void Push       (const std::string& a_id, const std::string& a_payload, const std::map<std::string, std::string>& a_args, const size_t& a_ttr, const size_t& a_validity);
            void Disconnect ();
            
        private: // Method(s) / Function(s)
            
            void EnsureConnection (const char* const a_action);
            
        }; // end of class 'Redis'

    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_BEANSTALK_H_
