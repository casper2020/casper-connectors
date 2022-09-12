/**
 * @file logger.h
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
#ifndef NRS_EV_LOGGER_H_
#define NRS_EV_LOGGER_H_

#include "cc/logs/logger.h"

#include "ev/loggable.h"

#include "cc/singleton.h"

#include "cc/utc_time.h"

#include <exception>
#include <string>
#include <map>
#include <limits.h>
#include <stdarg.h>

namespace ev
{
    
    /**
     * @brief A singleton to log messages.
     */
    class Logger final : public ::cc::logs::Logger, public cc::Singleton<::ev::Logger, cc::logs::OneShotInitializer>
    {                

    public: // Log API - Method(s) / Function(s)
        
        void Log (const std::string& a_token, const Loggable::Data& a_data, const char* a_format, ...) __attribute__((format(printf, 4, 5)));
        
    }; // end of class Logger
    
    /**
     * @brief Output a log message if the provided token is registered.
     *
     * @param a_token The token to be tested.
     * @param a_data
     * @param a_format
     * @param ...
     */
    inline void Logger::Log (const std::string& a_token, const Loggable::Data& a_data, const char* a_format, ...)
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
        
        // ... logger ...
        std::string module_name;
        if ( strlen(a_data.module()) > 22 ) {
            module_name = "..." + std::string(a_data.module() + ( strlen(a_data.module()) + 3 - 22));
        } else {
            module_name = a_data.module();
        }
        
        char prefix [256] = { 0 };
        snprintf(prefix, sizeof(prefix) / sizeof(prefix[0]),
                 "%8u, %15.15s, %22.22s, %32.32s, %p, ",
                 static_cast<unsigned>(getpid()),
                 a_data.ip_addr(),
                 module_name.c_str(),
                 a_data.tag(),
                 a_data.owner_ptr()
        );
        
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
                if ( true == EnsureBufferCapacity(static_cast<size_t>(aux) + sizeof(char)) ) {
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
            fprintf(tokens_.find(a_token)->second->fp_, "%s,%s",
                    cc::UTCTime::NowISO8601WithTZ().c_str(), prefix
            );
            fprintf(tokens_.find(a_token)->second->fp_, "%s\n", buffer_);
            // ... flush ...
            if ( stdout != file && stderr != file ) {
                fflush(file);
            }
        }
    }      
    
} // end of namespace 'ev'

#endif // NRS_EV_LOGGER_H_
