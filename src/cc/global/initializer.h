/**
* @file initializer.h
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
* along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#ifndef NRS_CC_GLOBAL_INITIALIZER_H_
#define NRS_CC_GLOBAL_INITIALIZER_H_

#include "cc/singleton.h"

#include "cc/macros.h"
#include "cc/debug/types.h"

#include <set>
#include <vector>
#include <map>
#include <string>
#include <functional> // std::function

#include "ev/loggable.h"

#include "cc/exception.h"

#include "cc/global/types.h"

namespace cc
{

    namespace global
    {
    
        // ---- //
        class Initializer;
        class OneShot final : public ::cc::Initializer<Initializer>
        {
            
        public: // Constructor(s) / Destructor
            
            OneShot (::cc::global::Initializer& a_instance);
            virtual ~OneShot ();
            
        }; // end of class 'OneShot'
        
        // ---- //
        class Initializer final : public cc::Singleton<Initializer, OneShot>
        {

            friend class OneShot;

        public: // Data Type(s)
                                 
            typedef struct {
                const std::function<void(const Process&, const Directories&, const void*, ::cc::global::Logs& )>& function_;
                const void*                                                                                       args_;
            } WarmUpNextStep;
            
            typedef struct {
                const std::set<int>      register_;
                std::function<bool(int)> unhandled_signals_callback_;
            } Signals;
            
            typedef struct {
                const bool required_;
                const bool runs_on_main_thread_;
            } V8;

            typedef struct {
                std::function<void(std::function<void()>)> call_on_main_thread_;
                std::function<void(const cc::Exception&)>  on_fatal_exception_;
            } Callbacks;

        private: // Data

            Process*              process_;
            Directories*          directories_;
            ::ev::Loggable::Data* loggable_data_;
            bool                  warmed_up_;
            bool                  initialized_;
            V8*                   v8_config_;

            bool                  being_debugged_;
            FILE*                 stdout_fp_;
            FILE*                 stderr_fp_;

        public: // Method(s) / Function(s)
            
            void WarmUp   (const Process& a_process, const Directories* a_directories, const Logs& a_logs, const V8& a_v8,
                           const WarmUpNextStep& a_next_step,
                           const std::function<void(std::string&, std::map<std::string, std::string>&)> a_present,
                           const std::set<std::string>* a_debug_tokens,
#ifdef __APPLE__
                           const bool a_use_local_dirs = true,
#else
                           const bool a_use_local_dirs = false,
#endif
                           const std::string a_log_fn_component = "");

            void Startup  (const Signals& a_signals, const Callbacks& a_callbacks);
            void Shutdown (bool a_for_cleanup_only);
            
        public: // Method(s) / Function(s)
            
            bool                IsInitialized   () const;
            bool                IsBeingDebugged () const;
            
            ev::Loggable::Data& loggable_data   ();
            
            const Directories& directories () const;
            
        private: //
            
            void EnableLogsIfRequired (const Logs& a_logs);
            
        public: // Static Method(s) / Function(s)
            
            static bool SupportsV8 ();

        }; // end of class 'Initializer'

        /**
         * @return True if this singleton is initialized, false otherwise.
         */
        inline bool Initializer::IsInitialized () const
        {
            return initialized_;
        }
    
        /**
         * @return True if the process where this singleton is running is being debugged, false otherwise.
         */
        inline bool Initializer::IsBeingDebugged () const
        {
            return being_debugged_;
        }

        /**
         * @return Global loggable data reference.
         */
        inline ::ev::Loggable::Data& Initializer::loggable_data ()
        {
            return *loggable_data_;
        }
    
        /**
         * @return R/O access to directories configs.
         */
        inline const Directories& Initializer::directories () const
        {
            CC_DEBUG_ASSERT(nullptr != directories_);
            return *directories_;
        }

    } // end of namespace 'global'

} // end of namespace 'cc'

#endif // NRS_CC_GLOBAL_INITIALIZER_H_
