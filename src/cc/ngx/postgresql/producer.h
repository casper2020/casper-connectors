/**
 * @file producer.h
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
#ifndef NRS_CC_NGX_POSTGRESQL_PRODUCER_H_
#define NRS_CC_NGX_POSTGRESQL_PRODUCER_H_

#include "cc/postgresql/offloader/producer.h"

namespace cc
{

    namespace ngx
    {
    
        namespace postgresql
        {
            
            class Producer final : public ::cc::postgresql::offloader::Producer
            {

            public: // Constructor(s) / Destructor
                
                Producer () = delete;
                Producer (::cc::postgresql::offloader::Queue& a_queue);
                virtual ~Producer();
                
            }; // end of class 'Offloader'

        } // end of namespace 'postgresql'
        
    } // end of namespace 'ngx'

}  // end of namespace 'cc'

#endif // NRS_CC_NGX_POSTGRESQL_PRODUCER_H_
