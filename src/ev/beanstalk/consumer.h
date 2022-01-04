/**
 * @file consumer.h
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
#ifndef NRS_EV_BEANSTALK_CONSUMER_H_
#define NRS_EV_BEANSTALK_CONSUMER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/beanstalk/config.h"

#include "beanstalk-client/beanstalk.hpp"

#include <string>
#include <functional>

namespace ev
{
    
    namespace beanstalk
    {
        
        class Consumer final : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        public: // Data Type(s)
            
            typedef struct {
                const std::function<void(const uint64_t&, const uint64_t&, const float&)>&       attempt_;
                const std::function<void(const uint64_t&, const uint64_t&, const std::string&)>& failure_;
            } ConnectCallbacks;
            
        private: //
            
            ::Beanstalk::Client* client_;
            
        public: // Constructor(s) / Destructor
            
            Consumer ();
            virtual ~Consumer ();
            
        public: // Method(s) / Function(s)
            
            void Connect (const ev::beanstalk::Config& a_config, const ConnectCallbacks& a_callbacks, volatile bool& a_abort);
            void Ignore  (const ev::beanstalk::Config& a_config);
            bool Reserve (::Beanstalk::Job& a_job);
            bool Reserve (::Beanstalk::Job& a_job, uint32_t a_timeout_sec);
            bool Bury    (const ::Beanstalk::Job& a_job, uint32_t a_priority = 1);
            bool Del     (const ::Beanstalk::Job& a_job);
            
        }; // end of class 'Consumer';
        
        inline bool Consumer::Reserve (::Beanstalk::Job& a_job)
        {
            return client_->reserve(a_job);
        }
        
        inline bool Consumer::Reserve (::Beanstalk::Job& a_job, uint32_t a_timeout_sec)
        {
            return client_->reserve(a_job, a_timeout_sec);
        }
        
        inline bool Consumer::Bury (const ::Beanstalk::Job& a_job, uint32_t a_priority)
        {
            return client_->bury(a_job, a_priority);
        }
        
        inline bool Consumer::Del (const ::Beanstalk::Job& a_job)
        {
            return client_->del(a_job);
        }
        
    } // end of namespace 'beanstalk'
    
} // end of namespace 'ev'

#endif // NRS_EV_BEANSTALK_CONSUMER_H_
