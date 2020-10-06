/**
 * @file reply.cc
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

#include "ev/curl/reply.h"

#include <inttypes.h>

#include "osal/osal_time.h"

#include <limits> // std::numeric_limits

/**
 * @brief Default constructor.
 *
 * @param a_code
 * @param a_headers
 * @param a_body
 * @param a_rtt
 */
ev::curl::Reply::Reply (int a_code, const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body,
                        size_t a_rtt)
    : ev::curl::Object(ev::curl::Object::Type::Reply),
    value_(a_code, a_headers, a_body, a_rtt)
{
    const auto last_modified_it = a_headers.find("Last-Modified");
    if ( a_headers.end() != last_modified_it && last_modified_it->second.size() > 0 ) {
        //
        // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Last-Modified
        //
        // Last-Modified: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT
        // ...
        // Greenwich Mean Time. HTTP dates are always expressed in GMT, never in local time.
        //
        const std::string last_modified = last_modified_it->second[0];
        if ( 29 == last_modified.length() ) {
            char     day_abbr   [4] = {0};
            uint8_t  day            = 0;
            char     month_abbr [4] = {0};
            uint16_t year           = 0;
            uint8_t  hour           = 0;
            uint8_t  minute         = 0;
            uint8_t  second         = 0;
            if ( 7 == sscanf(last_modified.c_str(), "%3c, %02" SCNu8 " %3c %04" SCNu16 " %02" SCNu8 ":%02" SCNu8 ":%02" SCNu8 " GMT", day_abbr, &day, month_abbr, &year, &hour, &minute, &second) ) {
                const uint8_t month = osal::Time::GetNumericMonth(month_abbr);
                if ( std::numeric_limits<uint8_t>::max() != month ) {
                    osal::Time::HumanReadableTime hrt;
                    hrt.seconds_    = second;
                    hrt.minutes_    = minute;
                    hrt.hours_      = hour;
                    hrt.year_       = year;
                    hrt.day_        = day;
                    hrt.weekday_    = std::numeric_limits<uint8_t>::max();
                    hrt.month_      = month;
                    hrt.tz_hours_   = 0;
                    hrt.tz_minutes_ = 0;
                    value_.set_last_modified(osal::Time::GetUtcEpochFromHumanReadableTime(hrt));
                }
            }
        }
    }
}

/**
 * @brief Destructor.
 */
ev::curl::Reply::~Reply ()
{
    /* empty */
}
