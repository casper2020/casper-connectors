/**
 * @file utc_time.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_UTC_TIME_H_
#define NRS_CC_UTC_TIME_H_

#include <sys/types.h> // int64_t
#include <stdint.h>    // uint8_t, etc

#include <string>      // std::string

namespace cc
{
    
    class UTCTime final
    {
        
    public: // Data Type(s)
        
        struct HumanReadable {
            uint8_t	 seconds_;      /* seconds after the minute [0-60] */
            uint8_t	 minutes_;      /* minutes after the hour [0-59]   */
            uint8_t  hours_;        /* hours since midnight [0-23]     */
            uint8_t  day_;          /* day of the month [1-31]         */
            uint8_t  month_;        /* months since January [1-12]     */
            uint16_t year_;         /* years since 1970...2038         */
        };
        
    public: // Static Method(s) / Function(s)
        
        static int64_t       Now                ();
        static int64_t       OffsetBy           (int64_t a_offset);
        static HumanReadable ToHumanReadable    (const int64_t a_epoch);
        static std::string   NowISO8601DateTime ();
        static std::string   NowISO8601WithTZ   ();
        
    }; // end of class 'Time'
    
} // end of namespace 'c''

#endif // NRS_CC_UTC_TIME_H_
