/**
 * @file date_time.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_DATE_TIME_H_
#define NRS_CC_DATE_TIME_H_

#include "cc/exception.h"

#include <time.h> // strptime

namespace cc
{

    namespace time
    {

        static time_t PARSE_YYMMDDHHMMSSZ (const char* const a_value)
        {
            struct tm tm; memset(&tm, 0, sizeof(tm));
            char* ptr;
            if ( nullptr == ( ptr = strptime(const_cast<char*>(a_value), "%y%m%d%H%M%SZ", &tm) ) ) {
                if ( nullptr == ( ptr = strptime(const_cast<char*>(a_value), "%y%m%d%H%M%S", &tm) ) ) {
                    throw ::cc::Exception("Date format of '%s' not supported!", a_value);
                }
            }
            // ... tm_gmtoff offset ...
            while ( ' ' == ptr[0] && '\0' != ptr[0] ) {
                ptr++;
            }
            if ( '-' == ptr[0] || '+' == ptr[0] ) {
                bool negative = ( *ptr++ == '-' );
                int o = 0;
                int d = 0;
                // ... read offset
                while ( d < 4 && *ptr>= '0' && *ptr<= '9' ) {
                    o = o * 10 + *ptr++ - '0';
                    ++d;
                }
                // ... convert to seconds ...
                if ( 2 == d ) {
                    o *= 100;
                } else if ( 4 == d ) {
                    if ( o % 100 < 60 ) {
                        o = ( o / 100 ) * 100 + ( ( o % 100 ) * 50 ) / 30;
                    }
                } else {
                    throw ::cc::Exception("gmtoff format of '%s' not supported!", a_value);
                }
                if ( o <= 1200 ) {
                    tm.tm_gmtoff = ( o * 3600 ) / 100;
                    if ( negative ) {
                        tm.tm_gmtoff = -tm.tm_gmtoff;
                    }
                } else {
                    throw ::cc::Exception("gmtoff format of '%s' not supported!", a_value);
                }
            }
            // ... done ...
            return timegm(&tm);
        }

    } // end of namespace 'time'
    
} // end of namespace 'cc'

#endif // NRS_CC_DATE_TIME_H_
