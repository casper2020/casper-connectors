/**
 * @file logger.h
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
#pragma once
#ifndef NRS_CC_POSTGRESQL_OFFLOADER_LOGGER_H_
#define NRS_CC_POSTGRESQL_OFFLOADER_LOGGER_H_

#include "cc/singleton.h"

#include "cc/logs/logger.h"

#include <regex> // std::regex_replace

namespace cc
{
    
    namespace postgresql
    {
        
        namespace offloader
        {
            
            // ---- //
            class Logger;
            class LoggerOneShot final : public ::cc::Initializer<Logger>
            {
                
            public: // Constructor(s) / Destructor
                
                LoggerOneShot (Logger& a_instance);
                virtual ~LoggerOneShot ();
                
            }; // end of class 'OneShot'
            
            // ---- //
            class Logger final : public ::cc::logs::Logger, public cc::Singleton<Logger, LoggerOneShot>
            {
                
                friend class LoggerOneShot;
                
            public: // Method(s) / Function(s)
                
                void Log (const char* const a_token, const char* a_format, ...) __attribute__((format(printf, 3, 4)));
                
            }; // end of class 'Logger'
            
        } // end of namespace 'offloader'
        
    } // end of namespace 'postgresql'
    
} // end of namespace 'cc'

#define CC_POSTGRESQL_OFFLOADER_LOG_RECYCLE() \
    ::cc::postgresql::offloader::Logger::GetInstance().Recycle()

#define CC_POSTGRESQL_OFFLOADER_LOG_REGISTER(a_token, a_uri) \
    ::cc::postgresql::offloader::Logger::GetInstance().Register(a_token, a_uri)

#define CC_POSTGRESQL_OFFLOADER_LOG_UNREGISTER(a_token) \
    ::cc::postgresql::offloader::Logger::GetInstance().Unregister(a_token)

#define CC_POSTGRESQL_OFFLOADER_LOG_MSG(a_token, a_format, ...) \
    ::cc::postgresql::offloader::Logger::GetInstance().Log(a_token, \
        "%s, " UINT64_FMT_LP(8) ", " a_format "\n", \
        cc::UTCTime::NowISO8601DateTime().c_str(), (uint64_t)getpid() ,__VA_ARGS__ \
);

#endif // NRS_CC_POSTGRESQL_OFFLOADER_LOGGER_H_
