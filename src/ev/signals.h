/**
 * @file signals.h
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
#ifndef NRS_EV_SIGNALS_H_
#define NRS_EV_SIGNALS_H_

#include "osal/osal_singleton.h"

#include "ev/scheduler/scheduler.h"
#include "ev/bridge.h"

#include <set>
#include <map>
#include <functional>

namespace ev
{
    
    /**
     * @brief A singleton to handle common signals
     */
    class Signals final : public osal::Singleton<Signals>, private scheduler::Client
    {
        
    private: // Static Const Ptrs
        
        static const Loggable::Data*                                    s_loggable_data_ptr_;
        
    private: // Static Ptrs

        static Bridge*                                                  s_bridge_ptr_;
        static std::function<void(const ev::Exception& a_ev_exception)> s_fatal_exception_callback_;
        static std::function<bool(int)>                                 s_unhandled_signal_callback_;
        
    private: // Data
        
        static std::map<int, std::vector<std::function<void(int)>>*>          s_other_signal_handlers_;
        
    public: // Method(s) / Function(s)
        
        void Startup    (const Loggable::Data& a_loggable_data_ref);
        void Register   (const std::set<int>& a_signals, std::function<bool(int)> a_callback);
        void Register   (Bridge* a_bridge_ptr, std::function<void(const ev::Exception&)> a_fatal_exception_callback);
        void Append     (const std::set<int>& a_signals, std::function<void(int)> a_callback);
        void Unregister ();
        bool OnSignal   (const int a_sig_no);
        
    private: //
        
        scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);
        
    }; // end of class 'Signals' 
    
}

#endif // NRS_EV_SIGNALS_H_
