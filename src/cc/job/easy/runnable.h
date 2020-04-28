/**
 * @file runnable.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_EASY_JOB_RUNNABLE_H_
#define NRS_CC_EASY_JOB_RUNNABLE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "json/json.h"

namespace cc
{

    namespace job
    {

        namespace easy
        {
        
            class Runnable : public cc::NonCopyable, public cc::NonMovable
            {
                
            public: // Constructor(s) / Destructor
                
                Runnable ();
                virtual ~Runnable ();
                
            public: // Pure Virtual Method(s) / Function(s)
                
                virtual void Run (const int64_t& a_id, const Json::Value& a_payload) = 0;
                
            }; // end of class 'Tube'
            
        } // end of namespace 'easy'
    
    } // end of namespace 'job'

} // end of namespace 'cc'
        
#endif // NRS_CC_EASY_JOB_RUNNABLE_H_
