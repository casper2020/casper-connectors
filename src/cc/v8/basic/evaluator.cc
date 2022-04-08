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

#include "cc/v8/basic/evaluator.h"

#include <iomanip> // std::setw

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow-field"

#include "unicode/ustring.h"
#include "unicode/unistr.h"
#include "unicode/decimfmt.h" // U_ICU_NAMESPACE::Locale ...
#include "unicode/datefmt.h"  // U_ICU_NAMESPACE::DateFormat
#include "unicode/smpdtfmt.h" // U_ICU_NAMESPACE::SimpleDateFormat
#include "unicode/dtfmtsym.h" // U_ICU_NAMESPACE::DateFormatSymbols
#include "unicode/schriter.h" // U_ICU_NAMESPACE::StringCharacterIterator

#pragma GCC diagnostic pop

const char* const cc::v8::basic::Evaluator::k_evaluate_basic_expression_func_name_ = "_basic_expr_eval";
const char* const cc::v8::basic::Evaluator::k_evaluate_basic_expression_func_ =
"function _basic_expr_eval(expr, $) {\n"
"    return eval(expr);\n"
"}"
;

const char* const cc::v8::basic::Evaluator::k_variable_dump_func_name_ = "_dump";
const char* const cc::v8::basic::Evaluator::k_variable_dump_func_ =
    "function _dump(title, $) {\n"
     "    NativeLog('----- [B] ' + title + ' ------');\n"
     "    NativeLog(JSON.stringify($));\n"
     "    NativeLog('----- [E] ' + title + ' ------');\n"
    "}"
;

/**
 * @brief Default constructor.
 *
 * @param a_owner
 * @param a_name
 * @param a_uri
 * @param a_out_path
 * @param a_name
 * @param a_functions
 */
cc::v8::basic::Evaluator::Evaluator (const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                                     const std::string& a_out_path,
                                     const cc::v8::Script::NativeFunctions& a_functions)
    : ::cc::v8::Script(a_owner, a_name, a_uri, a_out_path, a_functions),
    callable_ {
        /* ctx_      */ nullptr,
        /* name_     */ "",
        /* argc_     */ 3,
        /* argv_     */ args_,
        /* where_    */ "",
        /* on_error_ */ nullptr
    }
{
    callable_.on_error_ = std::bind(&cc::v8::basic::Evaluator::FunctionCallErrorCallback, std::placeholders::_1,  std::placeholders::_2);
}

/**
 * @brief Destructor.
 */
cc::v8::basic::Evaluator::~Evaluator ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Load this script to a specific context.
 *
 * @param a_external_scripts
 * @param a_expressions
 */
void cc::v8::basic::Evaluator::Load (const Json::Value& /* a_external_scripts */, const cc::v8::basic::Evaluator::Expressions& a_expressions)
{
    std::stringstream ss;
    
    // ... prepare script ...
    ss.str("");
    ss << "\"use strict\";\n";
    
    // ... clone object function ...
    ss << "\n//\n// " << k_evaluate_basic_expression_func_name_ << "\n//\n";
    ss << k_evaluate_basic_expression_func_;
        
    // ... dump function ...
    ss << "\n\n//\n// " << k_variable_dump_func_name_ << "\n//\n";
    ss << k_variable_dump_func_;
    
    // ... keep track of this function ...
    const std::string loaded_script = ss.str();
    
    IsolatedCall([this, &loaded_script]
                 (::v8::Local<::v8::Context>& /* a_context */, ::v8::TryCatch& /* a_try_catch */, ::v8::Isolate* a_isolate) {
                     
                     const ::v8::Local<::v8::String>                script    = ::v8::String::NewFromUtf8(a_isolate, loaded_script.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked();
                     const std::vector<::cc::v8::Context::Function> functions = {
                         { /* name  */ k_evaluate_basic_expression_func_name_   },
                         { /* name_ */ k_variable_dump_func_name_               }
                    };
                          
                    Compile(script, &functions);
                }
    );
}

/**
 * @brief Load a JSON string to current context.
 *
 * @param a_name
 * @param a_data
 * @param o_object
 * @param o_value
 * @param o_key
 */
void cc::v8::basic::Evaluator::SetData (const char* const a_name, const char* const a_data,
                                                 ::v8::Persistent<::v8::Object>* o_object, ::v8::Persistent<::v8::Value>* o_value,
                                                 ::v8::Persistent<::v8::String>* o_key) const
{
    IsolatedCall([&a_name, &a_data, &o_object, &o_value, &o_key]
                 (::v8::Local<::v8::Context>& a_context, ::v8::TryCatch& /* a_try_catch */, ::v8::Isolate* a_isolate) {
                                          ::v8::Persistent<::v8::Value> result;
                     
                     const ::v8::Local<::v8::String> key     = ::v8::String::NewFromUtf8(a_isolate, a_name, ::v8::NewStringType::kNormal).ToLocalChecked();
                     const ::v8::Local<::v8::String> payload = ::v8::String::NewFromUtf8(a_isolate, a_data, ::v8::NewStringType::kNormal).ToLocalChecked();
                     const ::v8::Local<::v8::Value>  value   = ::v8::JSON::Parse(a_context, payload).ToLocalChecked();
                            
                     ::v8::Local<::v8::Object> object = ::v8::Object::New(a_isolate);
                                                        
                     object->Set(a_context, key, value).Check();
                     if ( nullptr != o_object ) {
                         o_object->Reset(a_isolate, object);
                     }
                     if ( nullptr != o_value ) {
                         o_value->Reset(a_isolate, value);
                     }
                     if ( nullptr != o_key ) {
                         o_key->Reset(a_isolate, key);
                     }
            }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Call this function to evaluate an expression
 *
 * @param a_object     V8 Object, as \link ::v8::Value \link.
 * @param a_expression Expression to evaluate.
 *
 * @param o_value Evalutation result as \link ::cc::v8::Value \link.
 */

void cc::v8::basic::Evaluator::Evaluate (const ::v8::Persistent<::v8::Value>& a_object, const std::string& a_expr_string,
                                         ::cc::v8::Value& o_value)
{
    IsolatedCall([this, &a_expr_string, &a_object, &o_value]
                     (::v8::Local<::v8::Context>& a_context, ::v8::TryCatch& /* a_try_catch */, ::v8::Isolate* a_isolate) {
                         
                         const ::v8::Local<::v8::String> expr = ::v8::String::NewFromUtf8(a_isolate, a_expr_string.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked();
                         
                         callable_.ctx_      = &a_context;
                         callable_.name_     = k_evaluate_basic_expression_func_name_;
                         callable_.argc_     = 2;
                         callable_.argv_[0]  = /* expression  */ expr;
                         callable_.argv_[1]  = /* data object */ a_object.Get(a_isolate);
                         callable_.where_    = __PRETTY_FUNCTION__;
                         callable_.on_error_ = std::bind(&cc::v8::basic::Evaluator::FunctionCallErrorCallback, std::placeholders::_1,  std::placeholders::_2);
                        
                         o_value.SetNull();

                         CallFunction(callable_, result_);
                         
                         TranslateFromV8Value(a_isolate, result_, o_value);

                     }
        );
}

/**
 * @brief Dump an object.
 *
 * @param a_object
 */
void cc::v8::basic::Evaluator::Dump (const ::v8::Persistent<::v8::Value>& a_object) const
{
    IsolatedCall([this, &a_object]
                 (::v8::Local<::v8::Context>& a_context, ::v8::TryCatch& /* a_try_catch */, ::v8::Isolate* a_isolate) {
                     
                     ::v8::Persistent<::v8::Value> result;
                     ::v8::Local<::v8::Value>      args[2] = {};
                     
                     const ::cc::v8::Context::LoadedFunction::Callable callable {
                         /* ctx_      */ &a_context,
                         /* name_     */ k_variable_dump_func_name_,
                         /* argc_     */ ( sizeof(args) / sizeof(args[0]) ),
                         /* argv_     */ args,
                         /* where_    */ "Dump",
                         /* on_error_ */ std::bind(&FunctionCallErrorCallback, std::placeholders::_1, std::placeholders::_2)
                     };
                     
                     const ::v8::Local<::v8::String> title = ::v8::String::NewFromUtf8(a_isolate, "data", ::v8::NewStringType::kNormal).ToLocalChecked();
                     
                     args[0] = /* title       */ title;
                     args[1] = /* data object */ a_object.Get(a_isolate);
                     
                     CallFunction(callable, result);
                     
                 }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief The callback that is invoked by v8 whenever the JavaScript 'NativeLog' function is called.
 *
 * @param a_args
 */
void cc::v8::basic::Evaluator::NativeLog (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    if ( 0 == a_args.Length() ) {
        return;
    }
    const ::v8::HandleScope handle_scope(a_args.GetIsolate());
    for ( int i = 0; i < a_args.Length(); i++ ) {
        ::v8::String::Utf8Value str(a_args.GetIsolate(), a_args[i]);
        const char* cstr = *str;
        fprintf(stdout, " ");
        fprintf(stdout, "%s", cstr);
    }
    fprintf(stdout, " ");
    fprintf(stdout, "\n");
    fflush(stdout);
}

/**
 * @brief The callback that is invoked when a v8 call has failed.
 *
 * @param a_callable
 * @param a_message
 */
void cc::v8::basic::Evaluator::FunctionCallErrorCallback (const ::cc::v8::Context::LoadedFunction::Callable& a_callable, const char* const a_message)
{
    // TODO 2.0 v8
    fprintf(stdout, "---->\n");
    fprintf(stdout, "---- %s ----\n", "WARNING");
    fprintf(stdout, "---- When calling:\n");
    fprintf(stdout, "---- ---- function: %s\n", a_callable.name_);
    fprintf(stdout, "---- ---- argc    : %d\n", a_callable.argc_);
    fprintf(stdout, "%s\n", a_message);
    fprintf(stdout, "<----\n");
}
