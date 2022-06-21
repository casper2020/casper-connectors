/**
* @file logger.cc
*
* Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors.
*
* casper-connectors is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-connectors  is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cc/postgresql/offloader/logger.h"

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::postgresql::offloader::LoggerOneShot::LoggerOneShot (cc::postgresql::offloader::Logger& a_instance)
    : ::cc::Initializer<::cc::postgresql::offloader::Logger>(a_instance)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::LoggerOneShot::~LoggerOneShot ()
{
    /* empty */
}

// MARK: -

/**
 * @brief Output a log message if the provided token is registered.
 *
 * @param a_token The token to be tested.
 * @param a_format
 * @param ...
 */
void cc::postgresql::offloader::Logger::Log (const char* const a_token, const char* a_format, ...)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // ...if token is not registered...
    if ( tokens_.end() == tokens_.find(a_token) ) {
        // ... we're done ...
        return;
    }

    // ... ensure we have a valid buffer ...
    if ( false == EnsureBufferCapacity(1024) ) {
        // ... oops ...
        return;
    }

    int aux = INT_MAX;

    // ... try at least 2 times to construct the output message ...
    for ( uint8_t attempt = 0 ; attempt < 2 ; ++attempt ) {

        va_list args;
        va_start(args, a_format);
        aux = vsnprintf(buffer_, buffer_capacity_, a_format, args);
        va_end(args);

        if ( aux < 0 ) {
            // ... an error has occurred ...
            break;
        } else if ( aux > static_cast<int>(buffer_capacity_) ) {
            // ... realloc buffer ...
            if ( true == EnsureBufferCapacity(static_cast<size_t>(aux + sizeof(char))) ) {
                // ... last attempt to write to buffer ...
                continue;
            } else {
                // ... out of memory ...
                break;
            }
        } else {
            // ... all done ...
            break;
        }
    }

    // ... ready to output the message ? ...
    if ( aux > 0 && static_cast<size_t>(aux) < buffer_capacity_ ) {
        auto file = tokens_.find(a_token)->second->fp_;
        // ... output message ...
        fprintf(tokens_.find(a_token)->second->fp_, "%s", buffer_);
        // ... flush ...
        if ( stdout != file && stderr != file ) {
            fflush(file);
        }
    }
}
