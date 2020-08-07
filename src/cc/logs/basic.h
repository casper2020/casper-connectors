/**
 * @file basic.h
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
#ifndef NRS_CC_LOGS_BASIC_H_
#define NRS_CC_LOGS_BASIC_H_

#include "cc/singleton.h"

#include "cc/logs/logger.h"

namespace cc
{

    namespace logs
    {
    
        // ---- //
       class Basic;
       class OneShot final : public ::cc::Initializer<Basic>
       {
           
       public: // Constructor(s) / Destructor
           
           OneShot (::cc::logs::Basic& a_instance);
           virtual ~OneShot ();
           
       }; // end of class 'OneShot'
       
       // ---- //
       class Basic final : public ::cc::logs::Logger, public cc::Singleton<Basic, OneShot>
       {
           
           friend class OneShot;
               
       private: // Data
           
           bool initialized_;
           
       public: // Method(s) / Function(s)
           
           void Log (const char* const a_token, const char* a_format, ...) __attribute__((format(printf, 3, 4)));
           
       }; // end of class 'Basic'
    
    } // end of namespace 'logs'

} // end of namespace 'cc'

#endif // NRS_CC_LOGS_BASIC_H_
