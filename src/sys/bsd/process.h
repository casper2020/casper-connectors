/**
 * @file process.h
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
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NRS_SYS_BSD_PROCESS_H_
#define NRS_SYS_BSD_PROCESS_H_
#pragma once

#include "sys/process.h"

#include <sys/proc_info.h> // proc_pidinfo
#include <libproc.h>

namespace sys
{
    
    namespace bsd
    {
        
        class Process final : public ::sys::Process
        {
            
        public: // Constructor(s) / Destructor

            Process (Info&& a_info);
            virtual ~Process ();
            
        public: // Inherited Virtual Method(s) / Function(s) - Declaration
            
            virtual bool IsZombie      (const bool a_optional, bool& o_is_zombie);
            virtual bool IsRunning     (const bool a_optional, const pid_t a_parent_pid, bool& o_is_running);
            
        private: // Method(s) / Function(s)
            
            bool GetInfo (const bool a_optional, struct proc_bsdshortinfo& o_info);

        public: // Static Method(s)  / Function(s)
            
            static bool    IsProcessBeingDebugged (const pid_t& a_pid);
            static ssize_t MemPhysicalFootprint   (const pid_t& a_pid);

        }; // end of class 'Process'

    } // end of namespace 'bsd'
    
} // end of namespace 'sys'

#endif // NRS_SYS_BSD_PROCESS_H_
