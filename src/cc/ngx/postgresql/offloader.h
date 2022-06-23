/**
 * @file offloader.h
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
#ifndef NRS_CC_NGX_POSTGRESQL_OFFLOADER_H_
#define NRS_CC_NGX_POSTGRESQL_OFFLOADER_H_

#include "cc/postgresql/offloader/supervisor.h"

#include "cc/ngx/postgresql/producer.h"
#include "cc/ngx/postgresql/consumer.h"

#include <string>
#include <set>

namespace cc
{

    namespace ngx
    {
    
        namespace postgresql
        {

            class Offloader : public ::cc::postgresql::offloader::Supervisor
            {
                
            private: // Helper(s)
                
                ::cc::ngx::postgresql::Producer* ngx_producer_;
                ::cc::ngx::postgresql::Consumer* ngx_consumer_;
                
            private: // Data
                
                std::string                      consumer_socket_fn_;
                Consumer::FatalExceptionCallback consumer_fe_callback_;
                bool                             allow_start_call_;
                
            public: // Static Const DAta
                
                static const std::set<std::string> sk_known_logger_tokens_;

            public: // Constructor(s) / Destructor
                
                Offloader ();
                virtual ~Offloader ();
                
            public: // Method(s) / Function(s) - One Shot Call Only
                
                void Startup (const std::string& a_name,
                              const Offloader::Config& a_config,
                              const std::string& a_socket_fn, Consumer::FatalExceptionCallback a_callback);

            public: // Overloaded Virtual Method(s) / Function(s) - One Shot Call Only
                
                virtual void Start (const std::string& a_name, const Config& a_config);

            protected: // Inherited Virtual Method(s) / Function(s)
                
                virtual Pair Setup     (::cc::postgresql::offloader::Queue& a_queue);
                virtual void Dismantle (const Pair& a_pair);
                
            }; // end of class 'Offloader'

        } // end of namespace 'postgresql'
            
    } // end of namespace 'ngx'

}  // end of namespace 'cc'

#endif // NRS_CC_NGX_POSTGRESQL_OFFLOADER_H_
