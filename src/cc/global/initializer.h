/**
* @file initializer.h
*
* Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors.
*
* casper-connectors. is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-connectors.  is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-connectors..  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#ifndef NRS_CC_GLOBAL_INITIALIZER_H_
#define NRS_CC_GLOBAL_INITIALIZER_H_

#include "cc/singleton.h"

#include <set>
#include <vector>
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

        public: // Method(s) / Function(s)
            
            void WarmUp   (const Process& a_process, const Directories* a_directories, const Logs& a_logs, const V8& a_v8,
                           const WarmUpNextStep& a_next_step,
                           const std::set<std::string>* a_debug_tokens);
            void Startup  (const Signals& a_signals, const Callbacks& a_callbacks);
            void Shutdown (bool a_for_cleanup_only);
            
        public: // Method(s) / Function(s)
            
            bool                IsInitialized () const;
            ev::Loggable::Data& loggable_data ();
            
        private: //
            
            void EnableLogsIfRequired (const Logs& a_logs);
            void Log                  (const char* a_token, const char* a_format, ...) __attribute__((format(printf, 3, 4)));
            
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
         * @return Global loggable data reference.
         */
        inline ::ev::Loggable::Data& Initializer::loggable_data ()
        {
            return *loggable_data_;
        }

    } // end of namespace 'global'

} // end of namespace 'cc'

#endif // NRS_CC_GLOBAL_INITIALIZER_H_
