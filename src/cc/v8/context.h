/**
 * @file context.h
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
#ifndef NRS_CASPER_V8_CONTEXT_H_
#define NRS_CASPER_V8_CONTEXT_H_

#include "cc/v8/includes.h"

#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>


#if ( defined(CASPER_V8_CHRONO_ENABLED) || defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
    #define CASPER_V8_CHRONO_START(a_name) \
        const auto debug_##a_name## _start_tp = std::chrono::high_resolution_clock::now();
    #define CASPER_V8_CHRONO_ELPASED(a_name)[&]() -> size_t { \
        const auto debug_##a_name##_end_tp = std::chrono::high_resolution_clock::now(); \
        return static_cast<size_t>(std::chrono::duration_cast<std::chrono::microseconds>( debug_##a_name##_end_tp - debug_##a_name##_start_tp ).count()); \
    }()
    #define CASPER_V8_CHRONO_END(a_name, a_format, ...)[&]() -> size_t { \
        const auto debug_##a_name##_end_tp = std::chrono::high_resolution_clock::now(); \
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>( debug_##a_name##_end_tp - debug_##a_name##_start_tp ).count(); \
        fprintf(stdout, \
                "%s:%d\n\tTook %lld us to " a_format "\n", \
                __PRETTY_FUNCTION__, __LINE__, \
                elapsed, \
                __VA_ARGS__ \
        ); \
        return static_cast<size_t>(elapsed); \
    }()
    #define CASPER_V8_SILENCE_UNUSED_VARIABLE __attribute__((unused))
#else
    #undef CASPER_V8_CHRONO_START
    #define CASPER_V8_CHRONO_START(a_name)
    #undef CASPER_V8_CHRONO_ELPASED
    #define CASPER_V8_CHRONO_ELPASED(a_name)
    #undef CASPER_V8_CHRONO_END
    #define CASPER_V8_CHRONO_END(a_name, a_format, ...)
    #undef CASPER_V8_SILENCE_UNUSED_VARIABLE
#endif


namespace cc
{
    
    namespace v8
    {
        
        class Context
        {
            
        public: // Data Type(s)
            
            typedef struct
            {
                const char* const name_;
            } Function;
            
            typedef std::vector<Function>                            FunctionsVector;
            
        public: // Data Type(s)
            
            class LoadedFunction
            {
                
            public: // Data Type(s)
                
                typedef struct _Callable {
                    
                    ::v8::Local<::v8::Context>*                                                          ctx_;
                    const char*                                                                          name_;
                    int                                                                                  argc_;
                    ::v8::Local<::v8::Value>*                                                            argv_;
                    const char*                                                                          where_;
                    std::function<void(const struct _Callable& a_callback, const char* const a_message)> on_error_;

                } Callable;
                
            public: // Const Data
                
                const std::string name_;

            public:
                
                ::v8::Persistent<::v8::Function> f_;
                
            public: // Constructor(s) / Destructor
                
                LoadedFunction (const char* const a_name)
                    : name_(a_name)
                {
                    /* empty */
                }
                
            } ;
            
            typedef std::map<std::string, LoadedFunction*> LoadedFunctionsMap;
            
            typedef std::function<void(::v8::Local<::v8::Context>&, ::v8::TryCatch&, ::v8::Isolate*)> IsolatedCallback;
            typedef std::map<std::string, ::v8::FunctionCallback>        NativeFunctions;
            
        protected: // Data
            
            ::v8::Isolate*                  isolate_ptr_;
            ::v8::Persistent<::v8::Context> context_;
            ::v8::Persistent<::v8::Script>  script_;           // TODO do we need this?
            
        private: // Data
            
            LoadedFunctionsMap functions_;
            std::stringstream  tmp_trace_ss_;
            
        public: // Constructor(s) / Destructor
            
            Context () = delete;
            Context (::v8::Isolate* a_isolate_ptr, const NativeFunctions& a_functions);
            virtual ~Context ();
            
        public: // Method(s) / Function(s)
            
            bool Parse         (const std::string& a_uri,
                                ::v8::Persistent<::v8::Object>& o_object, ::v8::Persistent<::v8::Value>& o_data) const;
            
            bool Parse         (const char* const a_data,
                                ::v8::Persistent<::v8::Object>& o_object, ::v8::Persistent<::v8::Value>& o_data) const;
            
            void Compile       (const std::string& a_name,
                                const ::v8::Local<::v8::String>& a_script, const FunctionsVector* a_functions = nullptr);            
            void LoadFunctions (const LoadedFunction::Callable& a_callable, const FunctionsVector& a_functions);
            void CallFunction  (const LoadedFunction::Callable& a_callable, ::v8::Persistent<::v8::Value>& o_result) const;            
            void IsolatedCall  (const IsolatedCallback a_callback) const;
            
        private: // Method(s) / Function(s)
            
            static void LoadFunctions (::v8::Local<::v8::Context>& a_context, ::v8::Context::Scope& a_scope, ::v8::TryCatch& a_try_catch,
                                       const FunctionsVector& a_functions, LoadedFunctionsMap& o_functions);
            
            bool TraceException (::v8::TryCatch* a_try_catch, std::string& o_trace) const;
            void ThrowException (::v8::TryCatch* a_try_catch) const;
            
        }; // end of class 'ClientContext'
        
    } // end of namespace 'v8'
    
} // end of namespace 'cc'

#endif // NRS_CASPER_V8_CONTEXT_H_
