/**
 * @file types.h
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
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_GLOBAL_TYPES_H_
#define NRS_CC_GLOBAL_TYPES_H_

#include <string>
#include <vector>

namespace cc
{

    namespace global
    {
    
        typedef struct Process {
            const std::string name_;
            const std::string alt_name_;
            const std::string abbr_;
            const std::string version_;
            const std::string rel_date_;
            const std::string rel_branch_;
            const std::string rel_hash_;
            const std::string info_;
            const std::string banner_;
            const pid_t       pid_;
            const bool        standalone_;
            const bool        is_master_;
        } Process;
    
        typedef struct {
            const std::string etc_;
            const std::string log_;
            const std::string share_;
            const std::string run_;
            const std::string lock_;
            const std::string tmp_;
        } Directories;
    
        typedef struct {
           const std::string token_;
           const std::string uri_;
           const bool        conditional_;
           const bool        enabled_;
           const uint8_t     version_;
        } Log;
    
        typedef std::vector<Log> Logs;

    } // end of namespace 'global'

} // end of namespace 'cc'

#endif // NRS_CC_GLOBAL_TYPES_H_
