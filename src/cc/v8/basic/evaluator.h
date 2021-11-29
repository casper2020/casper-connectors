/**
 * @file script.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CASPER_V8_BASIC_EVALUATOR_H_
#define NRS_CASPER_V8_BASIC_EVALUATOR_H_

#include "cc/v8/script.h"
#include "cc/v8/value.h"

#include "cc/macros.h"

#include "cc/non-movable.h"
#include "cc/non-copyable.h"

namespace cc
{
    
    namespace v8
    {
        
        namespace basic
        {
            
            class Evaluator : public ::cc::v8::Script, public ::cc::NonMovable, public ::cc::NonCopyable
            {

            private: // Static Const Data

                static const char* const k_evaluate_basic_expression_func_name_;
                static const char* const k_evaluate_basic_expression_func_;

                static const char* const k_variable_dump_func_name_;
                static const char* const k_variable_dump_func_;

            private: // Data
                
                ::v8::Local<::v8::Value>                     args_[5];
                ::cc::v8::Context::LoadedFunction::Callable  callable_;
                ::v8::Persistent<::v8::Value>                result_;

            public: // Constructor(s) / Destructor
                
                Evaluator () = delete;
                Evaluator (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                           const std::string& a_out_path, const NativeFunctions& a_functions);
                virtual ~Evaluator ();
                
            public: // Inherited Method(s) / Function(s) - from ::cc::v8::Script
                
                virtual void Load (const Json::Value& a_external_scripts, const Expressions& a_expressions);
                                    
            public: // Method(s) / Function(s)
                
                void SetData   (const char* const a_name, const char* const a_data,
                                ::v8::Persistent<::v8::Object>* o_object = nullptr,
                                ::v8::Persistent<::v8::Value>* o_value = nullptr,
                                ::v8::Persistent<::v8::String>* o_key = nullptr) const;
                
                void Dump     (const ::v8::Persistent<::v8::Value>& a_object) const;
                void Evaluate (const ::v8::Persistent<::v8::Value>& a_object, const std::string& a_expr_string,
                               ::cc::v8::Value& o_value);
                
            private: // Static Method(s) / Function(s)
                
                static void NativeLog                 (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args);
                static void FunctionCallErrorCallback (const ::cc::v8::Context::LoadedFunction::Callable& a_callable, const char* const a_message);
                
            }; // end of class 'Evaluator'
                
        } // end of namespace 'basic'
        
    } // end of namespace 'v8'
    
} // end of namespace 'cc'

#endif // NRS_CASPER_V8_BASIC_EVALUATOR_H_
