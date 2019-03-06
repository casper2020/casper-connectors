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

#include "ev/beanstalk/config.h"

#include "beanstalk-client/beanstalk.hpp"

#include <string>

namespace ev
{
    
    namespace beanstalk
    {
        
        class Consumer
        {
            
        private: //
            
            ::Beanstalk::Client* client_;
            
        public: // Constructor(s) / Destructor
            
            Consumer (const Config& a_config);
            virtual ~Consumer ();
            
        public: // Method(s) / Function(s)
            
            bool Reserve (::Beanstalk::Job& a_job);
            bool Reserve (::Beanstalk::Job& a_job, uint32_t a_timeout_sec);
            bool Bury    (const ::Beanstalk::Job& a_job, uint32_t a_priority = 1);
            bool Del     (const ::Beanstalk::Job& a_job);
            
        private:  // Method(s) / Function(s)
            
            void Runner ();
            
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
