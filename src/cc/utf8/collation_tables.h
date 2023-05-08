/**
 * @file collation_tables.h
 *
 * Based on code originally developed for NDrive S.A.
 *
 * Copyright (c) 2011-2023 Cloudware S.A. All rights reserved.
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
 * along with osal.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CC_UTILS_UTF8_COLLATION_TABLES_H_
#define CC_UTILS_UTF8_COLLATION_TABLES_H_

#include "cc/types.h"

#define UTF8_COLLATION_MAX_INDEX 1792

namespace cc
{

    namespace utf8
    {
        
        class CollationTables
        {
            
        public: // static const data
            
            static const uint16_t k_utf8_collation_table_       [UTF8_COLLATION_MAX_INDEX];
            static const uint16_t k_utf8_collation_table_lower_ [UTF8_COLLATION_MAX_INDEX];
            static const uint16_t k_utf8_collation_table_upper_ [UTF8_COLLATION_MAX_INDEX];
            
        }; // end of class 'CollationTables'
        
    } // end of namespace 'utf8'
    
} // end of namespace 'cc'

#endif // CC_UTILS_UTF8_COLLATION_TABLES_H_
