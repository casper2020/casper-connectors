/**
 * @file client.h
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
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_CLIENT_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_CLIENT_H_

#include "cc/exception.h"

#include <functional>

namespace cc
{

    namespace postgresql
    {
    
        namespace offloader
        {
        
            class Client
            {

            public: // Constructor(s) / Destructor
                
                /**
                 * @brief Destructor.
                 */
                virtual ~Client()
                {
                    /* empty */
                }

            };                
        
        } // end of namespace 'offloader'
    
    } // end of namespace 'postgresql'

} // end of namespace 'cc'

#endif // NRS_CC_POSTGRESQL_OFFLOADER_CLIENT_H_
