/**
 * @file script.h
 *
 * Copyright (c) 2010-2018 Neto Ranito & Seabra LDA. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CASPER_CONNECTORS_V8_SCRIPT_H_
#define NRS_CASPER_CONNECTORS_V8_SCRIPT_H_

#include "cc/v8/value.h"
#include "cc/v8/context.h"

namespace cc
{
    
    namespace v8
    {
        
        class Script
        {

        public: // Data Type(s)
            
            typedef cc::v8::Context::LoadedFunction   LoadedFunction;
            typedef cc::v8::Context::FunctionsVector  FunctionsVector;
            typedef cc::v8::Context::IsolatedCallback IsolatedCallback;
            typedef cc::v8::Context::NativeFunctions  NativeFunctions;
            typedef std::vector<std::string>              Expressions;                        
            
        public: // Const Data
            
            const std::string owner_;
            const std::string name_;
            const std::string uri_;
            const std::string out_path_;
            
        private: // Data
            
            ::cc::v8::Context context_;
            bool              cancelled_;
            
        public: // Constructor(s) / Destructor
            
            Script () = delete;
            Script (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                    const std::string& a_out_path,
                    const NativeFunctions& a_functions);
            virtual ~Script ();
            
        public: // Method(s) / Function(s)
            
            virtual void Load (const Json::Value& a_external_scripts, const Expressions& a_expressions) = 0;
            
        public: // Inline Method(s) / Function(s)
            
            void CallFunction (const LoadedFunction::Callable& a_callable, ::v8::Persistent<::v8::Value>& o_result) const;
            void IsolatedCall (IsolatedCallback a_callback) const;
            
        protected: // Method(s) / Function(s)
            
            virtual void Compile             (const ::v8::Local<::v8::String>& a_script,
                                              const FunctionsVector* a_functions = nullptr);
            
            virtual void TranslateFromV8Value (::v8::Isolate* a_isolate, const ::v8::Persistent<::v8::Value>& a_value, Value& o_value) const;
            virtual void TranslateToV8Value   (::v8::Isolate* a_isolate, const Value& a_value, ::v8::Local<::v8::Value>& o_value) const;
                        
        }; // end of class 'Script'
        
        /**
         * @brief Execute a previously load function.
         *
         * @param a_callable
         *
         * @param o_result
         */
        inline void cc::v8::Script::CallFunction (const cc::v8::Script::LoadedFunction::Callable& a_callable, ::v8::Persistent<::v8::Value>& o_result) const
        {
            context_.CallFunction(a_callable, o_result);
        }
        
        /**
         * @brief Prepare scope for a v8 context.
         *
         * @param a_callback
         */
        inline void cc::v8::Script::IsolatedCall (cc::v8::Script::IsolatedCallback a_callback) const
        {
            context_.IsolatedCall(a_callback);
        }
        
    } // end of namespace 'v8'
    
} // end of namespace 'cc'

#endif // NRS_CASPER_CONNECTORS_V8_SCRIPT_H_
