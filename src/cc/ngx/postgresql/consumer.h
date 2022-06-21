/**
 * @file consumer.h
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

#pragma once
#ifndef NRS_CC_NGX_POSTGRESQL_CONSUMER_H_
#define NRS_CC_NGX_POSTGRESQL_CONSUMER_H_

#include "cc/postgresql/offloader/consumer.h"

#include "cc/ngx/event.h"

#include <mutex>

namespace cc
{

    namespace ngx
    {
    
        namespace postgresql
        {
            
            class Consumer final : public ::cc::postgresql::offloader::Consumer
            {
                
            public: // Data Type(s)
                
                typedef std::function<void(const ::cc::Exception&)> FatalExceptionCallback;
                
            private: // Threading
                
                std::mutex             mutex_;
                
            private: // Helper(s)
                
                ::cc::ngx::Event*      event_; //<! must be under mutex umbrella.

            private: // Data
                
                std::string            socket_fn_;
                FatalExceptionCallback fe_callback_;                

            public: // Constructor(s) / Destructor
                
                Consumer () = delete;
                Consumer (const std::string& a_socket_fn, FatalExceptionCallback a_callback);
                virtual ~Consumer();
                
            public: // Overloaded Virtual Method(s) / Function(s) - One Shot Call ONLY!
                
                virtual void Start (const std::string& a_name, ::cc::postgresql::offloader::Shared* a_shared);
                virtual void Stop  ();

            private: // Method(s) / Function(s)
                
                void Notify (const ::cc::postgresql::offloader::OrderResult* a_result);
                
            public: // Method(s) / Function(s) - One Shot Call ONLY!
                
                void Start (const std::string& a_socket_fn, const float& a_polling_timeout, Consumer::FatalExceptionCallback a_callback);

            }; // end of class 'Offloader'

        } // end of namespace 'postgresql'
        
    } // end of namespace 'ngx'

}  // end of namespace 'cc'

#endif // NRS_CC_NGX_POSTGRESQL_CONSUMER_H_
