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
            
            typedef struct Process {
                const std::string alt_name_;
                const std::string name_;
                const std::string version_;
                const std::string info_;
                const pid_t       pid_;
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
                const bool        conditional_;
                const bool        enabled_;
            } LogEntry;
            
            typedef struct {
                const std::vector<LogEntry> logger_;
                const std::vector<LogEntry> logger_v2_;
            } Logs;
            
            typedef struct {
                const std::function<void(const Process&, const Directories&, const void*)>& function_;
                const void*                                                                 args_;
            } Callback;
            
            typedef struct {
                const std::set<int>      register_;
                std::function<bool(int)> unhandled_signals_callback_;
            } Signals;
            
        private: // Data

            Process*              process_;
            Directories*          directories_;
            ::ev::Loggable::Data* loggable_data_;
            bool                  warmed_up_;
            bool                  initialized_;

        public: // Method(s) / Function(s)
            
            void WarmUp   (const Process& a_process, const Directories* a_directories, const Logs& a_logs, const Signals& a_signals,
                           const Callback& a_callback,
                           const std::set<std::string>* a_debug_tokens);
            void Startup  ();
            void Shutdown (bool a_for_cleanup_only);
            
        public: // Method(s) / Function(s)
            
            bool                IsInitialized () const;
            ev::Loggable::Data& loggable_data ();
            
        private: //
            
            void Log (const char* a_token, const char* a_format, ...) __attribute__((format(printf, 3, 4)));
            
        public: // Static Method(s) / Function(s)
            
            static bool SupportsV8 ();

        }; // end of class 'Initializer'
    
        /**
         * @return Global loggable data reference.
         */
        inline ::ev::Loggable::Data& Initializer::loggable_data ()
        {
            return *loggable_data_;
        }
    
        /**
         * @return True if this singleton is initialized, false otherwise.
         */
        inline bool Initializer::IsInitialized () const
        {
            return initialized_;
        }
                
    } // end of namespace 'global'

} // end of namespace 'cc'

#endif // NRS_CC_GLOBAL_INITIALIZER_H_
