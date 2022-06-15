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
                
            private: // Helper(s)
                
                ::cc::ngx::Event* event_;

            private: // Data
                
                bool allow_start_call_;

            public: // Constructor(s) / Destructor
                
                Consumer ();
                virtual ~Consumer();
                
            public: // Overloaded Virtual Method(s) / Function(s) - One Shot Call ONLY!
                
                virtual void Start (const float& a_polling_timeout);
                virtual void Stop  ();

            protected: // Inherited Virtual Method(s) / Function(s)
                
                virtual void Notify (const PGresult* a_result);
                
            public: // Method(s) / Function(s) - One Shot Call ONLY!
                
                void Start (const std::string& a_socket_fn, const float& a_polling_timeout, Consumer::FatalExceptionCallback a_callback);

            }; // end of class 'Offloader'

        } // end of namespace 'postgresql'
        
    } // end of namespace 'ngx'

}  // end of namespace 'cc'

#endif // NRS_CC_NGX_POSTGRESQL_CONSUMER_H_
