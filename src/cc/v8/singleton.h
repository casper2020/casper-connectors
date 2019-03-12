/**
 * @file singleton.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * jayscriptor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jayscriptor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with jayscriptor.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef NRS_CASPER_V8_SINGLETON_H_
#define NRS_CASPER_V8_SINGLETON_H_

#include "cc/v8/includes.h"

#include "cc/non-copyable.h"

namespace cc
{
    
    namespace v8
    {
        
        class Singleton final : protected ::cc::NonCopyable
        {
            
        public: // Static Data
            
            static Singleton& GetInstance()
            {
                static Singleton instance;
                return instance;
            }
            
        private: // Data
            
            bool                         initialized_;
            std::unique_ptr<::v8::Platform>              platform_;
            ::v8::Isolate*               isolate_;
            ::v8::Isolate::CreateParams  create_params_;
            
        public: // Constructor(s) / Destructor
            
                     Singleton ();
            virtual ~Singleton ();
            
        public: // Method(s) / Function(s) - Oneshot call only!!!
            
            void Startup  (const char* const a_exec_uri, const char* const a_icu_data_uri);
            void Shutdown ();
            void Initialize ();
            
        public: // Inline Method(s) / Function(s)
            
            ::v8::Isolate* isolate () const;
                    
        }; // end of class 'Singleton'
        
        inline ::v8::Isolate* Singleton::isolate () const
        {
            return isolate_;
        }
        
    } // end of namespace 'v8'
    
} // end of namespace 'cc'

#endif // NRS_CASPER_V8_SINGLETON_H_
