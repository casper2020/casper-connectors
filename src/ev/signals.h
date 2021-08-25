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

#include "cc/singleton.h"

#include "ev/scheduler/scheduler.h"

#include "ev/logger_v2.h"

#include <set>
#include <map>
#include <functional>

namespace ev
{
    
    // ---- //
    class Signals;
    class Initializer final : public ::cc::Initializer<Signals>
    {

    public: // Constructor(s) / Destructor

        Initializer (Signals& a_instance);
        virtual ~Initializer ();

    }; // end of class 'Initializer'
   
    // ---- //
    class Signals final : public cc::Singleton<Signals, Initializer>, private scheduler::Client
    {
        
        friend class Initializer;
        
    public: // Data Type(s)
        
        typedef struct {
            std::function<bool(int)>                   on_signal_;
            std::function<void(std::function<void()>)> call_on_main_thread_;
        } Callbacks;
        
        typedef struct {
            int         id_;
            std::string name_;
            std::string description_;
            std::string purpose_;
        } SignalInfo;
        
        typedef struct {
            int                          signal_;
            std::string                  description_;
            std::function<std::string()> callback_;
        } Handler;
        
    private: // Const Ptrs
        
        Loggable::Data*                      loggable_data_;
        LoggerV2::Client*                    logger_client_;
        
    private: //  Ptrs

        std::set<int>                        signals_;
        Callbacks                            callbacks_;
        
    private: // Data
        
        std::map<int, std::vector<Handler>*> other_signal_handlers_;
        std::vector<SignalInfo>              supported_;
        
    public: // Method(s) / Function(s)
        
        void WarmUp     (const Loggable::Data& a_loggable_data_ref);
        
        void Startup    (const std::set<int>& a_signals, Callbacks a_callbacks);
        
        void Shutdown   ();
        void Append     (const std::vector<Handler>& a_handlers);
        void Unregister ();
        
        bool OnSignal   (const int a_sig_no);
        
    public: // Inline Mehtod(s) / Function(s)
        
        const std::vector<SignalInfo>& Supported () const;
        
    private: //
        
        scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);
        
    }; // end of class 'Signals' 
    
    inline const std::vector<Signals::SignalInfo>& Signals::Supported () const
    {
        return supported_;
    }

}

#endif // NRS_EV_SIGNALS_H_
