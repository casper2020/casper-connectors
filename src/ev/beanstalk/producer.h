/**
 * @file producer.h
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
#ifndef NRS_EV_BEANSTALK_PRODUCER_H_
#define NRS_EV_BEANSTALK_PRODUCER_H_

#include "ev/beanstalk/config.h"

#include "beanstalk.hpp"

#include <string>

namespace ev
{
    
    namespace beanstalk
    {
        class Producer
        {
            
        private: //
            
            ::Beanstalk::Client* client_;
            
        public: // Constructor(s) / Destructor
            
            Producer (const ::ev::beanstalk::Config& a_config);
            Producer (const ::ev::beanstalk::Config& a_config, const std::string& a_tube);
            virtual ~Producer ();
            
        public: // Method(s) / Function(s)
            
            int64_t Put (const std::string& a_payload,
                         const uint32_t a_priority = 0, const uint32_t a_delay = 0, const uint32_t a_ttr = 60);
            int64_t Put (const char* const a_data, const size_t a_size,
                         const uint32_t a_priority = 0, const uint32_t a_delay = 0, const uint32_t a_ttr = 60);
            
        }; // end of class 'Producer';
        
        inline int64_t Producer::Put (const std::string& a_payload,
                                      const uint32_t a_priority, const uint32_t a_delay, const uint32_t a_ttr)
        {
            return client_->put(a_payload, a_priority, a_delay, a_ttr);
        }

        inline int64_t Producer::Put (const char* const a_data, const size_t a_size,
                                      const uint32_t a_priority, const uint32_t a_delay, const uint32_t a_ttr)
        {
            return client_->put(a_data, a_size, a_priority, a_delay, a_ttr);
        }

    } // end of namespace 'beanstalk'
    
} // end of namespace 'ev'

#endif // NRS_EV_BEANSTALK_PRODUCER_H_
