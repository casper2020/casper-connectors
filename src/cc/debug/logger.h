/**
 * @file logger.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_DEBUG_LOGGER_H_
#define NRS_CC_DEBUG_LOGGER_H_

#include "cc/singleton.h"

#include "cc/logs/logger.h"

namespace cc
{

    namespace debug
    {
    
        // ---- //
       class Logger;
       class OneShot final : public ::cc::Initializer<Logger>
       {
           
       public: // Constructor(s) / Destructor
           
           OneShot (::cc::debug::Logger& a_instance);
           virtual ~OneShot ();
           
       }; // end of class 'OneShot'
       
       // ---- //
       class Logger final : public ::cc::logs::Logger, public cc::Singleton<Logger, OneShot>
       {
           
           friend class OneShot;
                                 
       private: // Data
           
           bool initialized_;
           
       public: // Method(s) / Function(s)
           
           void Register (const std::string& a_token);
           void Log      (const char* const a_token, const char* a_format, ...) __attribute__((format(printf, 3, 4)));
           
       }; // end of class 'Basic'
    
    } // end of namespace 'logs'

} // end of namespace 'cc'

#endif // NRS_CC_DEBUG_LOGGER_H_
