/**
 * @file object.h - PostgreSQL
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
#ifndef NRS_EV_POSTGRESQL_OBJECT_H_
#define NRS_EV_POSTGRESQL_OBJECT_H_

#include "ev/object.h"

namespace ev
{
    
    namespace postgresql
    {
        
        class Object : public ev::Object
        {

        public: // Constructor(s) / Destructor
            
            Object (const Type& a_type);
            virtual ~Object ();
            
        }; // end of class 'Object'
        
    } // end of namespace 'postgresql'
    
} // end of namespace 'ev'

#endif // NRS_EV_POSTGRESQL_OBJECT_H_
