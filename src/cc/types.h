/**
 * @file types.h
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
#ifndef NRS_CC_TYPES_H_
#define NRS_CC_TYPES_H_

#include <string>

namespace cc
{
    
    
    typedef struct _Directories
    {
        
        std::string log_;
        std::string run_;
        std::string lock_;
        std::string shared_;
        std::string output_;
        
        inline void operator=(const _Directories& a_directories)
        {
            log_    = a_directories.log_;
            run_    = a_directories.run_;
            lock_   = a_directories.lock_;
            shared_ = a_directories.shared_;
            output_ = a_directories.output_;
        }
        
    } Directories;
    
}

#endif // NRS_CC_TYPES_H_
