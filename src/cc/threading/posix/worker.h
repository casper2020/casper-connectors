/**
 * @file worker.h
 *
 * Copyright (c) 2011-2010 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_THEADING_POSIX_WORKER_H_
#define NRS_CC_THEADING_POSIX_WORKER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <string>
#include <set>

namespace cc
{

    namespace threading
    {
    
        namespace posix
        {

            class Worker final : public NonCopyable, public NonMovable
            {
                
            public: // Static Method(s) / Function(s)
                
                static void SetName      (const std::string& a_name);
                static void BlockSignals (const std::set<int>& a_signals);
                
            }; // end of class 'Worker'

        } // end of namespace 'posix'
    
    } // end of namespace 'threading'

} // end of namespace 'cc'

#endif // NRS_CC_THEADING_POSIX_WORKER_H_
