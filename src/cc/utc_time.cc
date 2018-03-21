/**
 * @file utc_time.cc
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

#include "cc/utc_time.h"

#include "cc/exception.h"

#include <sys/time.h>
#include <time.h>
#include <ctime>

#include <chrono> // std::chrono

/**
 * @return Time since UNIX epoch.
 */
int64_t cc::UTCTime::Now ()
{
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

/**
 * @brief Retrieve time since unix epoch.
 *
 * @param a_offset
 *
 * @return Time since UNIX epoch.
 */
int64_t cc::UTCTime::OffsetBy (int64_t a_offset)
{
    return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + a_offset;
}

/**
 * @brief Convert a UNIX epoch value in to a human readable struct.
 *
 * @return Human readable struct.
 */
cc::UTCTime::HumanReadable cc::UTCTime::ToHumanReadable (const int64_t a_epoch)
{
    time_t tt     = (time_t)a_epoch;
    tm     utc_tm;
    
    if ( &utc_tm != gmtime_r(&tt, &utc_tm) ) {
        throw cc::Exception("Unable to convert epoch to human readable time!");
    }
    
    cc::UTCTime::HumanReadable hr;
    
    hr.seconds_ = static_cast<uint8_t >(utc_tm.tm_sec        ); /* seconds after the minute [0-60] */
    hr.minutes_ = static_cast<uint8_t >(utc_tm.tm_min        ); /* minutes after the hour [0-59]   */
    hr.hours_   = static_cast<uint8_t >(utc_tm.tm_hour       ); /* hours since midnight [0-23]     */
    hr.day_     = static_cast<uint8_t >(utc_tm.tm_mday       ); /* day of the month [1-31]         */
    hr.month_   = static_cast<uint8_t >(utc_tm.tm_mon  +    1); /* months since January [1-12]     */
    hr.year_    = static_cast<uint16_t>(utc_tm.tm_year + 1900); /* years since 1970...2038         */

    return hr;
}

/**
 * @return ISO8601WithTZ.
 */

std::string cc::UTCTime::NowISO8601WithTZ ()
{
    const cc::UTCTime::HumanReadable hr = cc::UTCTime::ToHumanReadable(cc::UTCTime::Now());
    
    char buff[27] = {0};
    const int w = snprintf(buff, 26, "%04d-%02d-%02dT%02d:%02d:%02d+%02d:%02d",
                           static_cast<int>(hr.year_ ), static_cast<int>(hr.month_  ), static_cast<int>(hr.day_    ),
                           static_cast<int>(hr.hours_), static_cast<int>(hr.minutes_), static_cast<int>(hr.seconds_),
                           static_cast<int>(0), static_cast<int>(0)
    );

    if ( w <=0 || w > 25 ) {
        throw cc::Exception("Unable to convert epoch to ISO8601WithTZ!");
    }

    return std::string(buff, w);
}
