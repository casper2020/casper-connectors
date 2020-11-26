/**
 * @file bridge.h
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
#ifndef NRS_EV_BRIDGE_H_
#define NRS_EV_BRIDGE_H_

#include "ev/exception.h"

#include <functional> // std::function

namespace ev
{

    class Bridge
    {
        
    public: // Pure Virtual Method(s) / Function(s)
        
        virtual void    CallOnMainThread    (std::function<void(void* a_payload)> a_callback, void* a_payload, int64_t a_timeout_ms = 0) = 0;
        virtual void    CallOnMainThread    (std::function<void()> a_callback,int64_t a_timeout_ms = 0)                                  = 0;
        virtual void    ThrowFatalException (const ev::Exception& a_ev_exception)                                                        = 0;
    
    public: // Optional Virtual Method(s) / Function(s)
        
        virtual void    Loop                (const bool /* a_at_main_thread */) { throw Exception("Not Implemented!"); }
        
    }; // end of class 'SharedHandler'

} // end of namespace 'ev'

#endif // NRS_EV_BRIDGE_H_
