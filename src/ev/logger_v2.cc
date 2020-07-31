/**
 * @file logger_v2.cc
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

#include "ev/logger_v2.h"

/**
 * @brief Calculate the number of digits for the provided value.
 *
 * @param a_value Value to be used.
 * 
 * @return Number of digits for provided value.
 */
size_t ev::LoggerV2::NumberOfDigits (size_t a_value)
{
    size_t count = 0;
    while ( 0 != a_value ) {
        a_value /= 10;
        count++;
    }
    return count;
}

/**
 * @brief Update this client logger prefix and enabled tokens.
 */
 void ev::LoggerV2::Client::UpdateLoggerPrefix ()
{
    std::string module_name;
    if ( strlen(loggable_data_ref_.module()) > 22 ) {
        module_name = "..." + std::string(loggable_data_ref_.module() + ( strlen(loggable_data_ref_.module()) + 3 - 22));
    } else {
        module_name = loggable_data_ref_.module();
    }
    std::string tag;
    if ( strlen(loggable_data_ref_.tag()) > 30 ) {
        tag = "..." + std::string(loggable_data_ref_.tag() + ( strlen(loggable_data_ref_.tag()) + 3 - 30));
    } else {
        tag = loggable_data_ref_.tag();
    }
    // ... logger ...
    char prefix [256] = { 0 };
    
    const size_t max = sizeof(prefix) / sizeof(prefix[0]);

    if ( LoggableFlags::Default == prefix_format_flags_ ) {
        snprintf(prefix, max,
                 "%8u, %15.15s, %22.22s, %32.32s, %p, ",
                 static_cast<unsigned>(getpid()),
                 loggable_data_ref_.ip_addr(),
                 module_name.c_str(),
                 tag.c_str(),
                 loggable_data_ref_.owner_ptr()
        );
    } else {
        char*  ptr = prefix;
        size_t len = max - ( 2 * sizeof(char) );
        size_t adv = 0;
        int    aux = 0;
        // ... PID ...
        if ( LoggableFlags::PID == ( prefix_format_flags_ & LoggableFlags::PID ) ) {
            aux = snprintf(ptr, len, "%8u", static_cast<unsigned>(getpid()));
            if ( aux > 0 ) { adv += static_cast<size_t>(aux); len -= static_cast<size_t>(std::max(0, aux)); } else { len = 0; }
        }
        // ... IP ADDR  ...
        if ( len > 0 && LoggableFlags::IPAddress == ( prefix_format_flags_ & LoggableFlags::IPAddress ) ) {
            aux = snprintf(ptr + adv, len, ", %15.15s", loggable_data_ref_.ip_addr());
            if ( aux > 0 ) { adv += static_cast<size_t>(aux); len -= static_cast<size_t>(std::max(0, aux)); } else { len = 0; }
        }
        // ... MODULE  ...
        if ( len > 0 && LoggableFlags::Module == ( prefix_format_flags_ & LoggableFlags::Module ) ) {
            aux = snprintf(ptr + adv, len, ", %22.22s", module_name.c_str());
            if ( aux > 0 ) { adv += static_cast<size_t>(aux); len -= static_cast<size_t>(std::max(0, aux)); } else { len = 0; }
        }
        // ... TAG  ...
        if ( len > 0 && LoggableFlags::Tag == ( prefix_format_flags_ & LoggableFlags::Tag ) ) {
            aux = snprintf(ptr + adv, len, ", %32.32s", tag.c_str());
            if ( aux > 0 ) { adv += static_cast<size_t>(aux); len -= static_cast<size_t>(std::max(0, aux)); } else { len = 0; }
        }
        // ... OWNER PTR  ...
        if ( len > 0 && LoggableFlags::OwnerPTR == ( prefix_format_flags_ & LoggableFlags::OwnerPTR ) ) {
            aux = snprintf(ptr + adv, len, ", %p", loggable_data_ref_.owner_ptr());
            if ( aux > 0 ) { adv += static_cast<size_t>(aux); len -= static_cast<size_t>(std::max(0, aux)); } else { len = 0; }
        }
        // ... SEPARATOR  ...
        (void)snprintf(ptr + adv, len, "%s", ", ");
    }
    prefix_ = prefix;
    if ( true == prefix_changed() ) {
        prefix_changes_count_ += 1;
    }
}
