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

#include "cc/fs/dir.h"

#include "cc/v8/exception.h"

const char* const cc::v8::basic::Evaluator::k_evaluate_basic_expression_func_name_ = "_basic_expr_eval";
const char* const cc::v8::basic::Evaluator::k_evaluate_basic_expression_func_ =
"function _basic_expr_eval(expr, $) {\n"
"    return eval(expr);\n"
"}"
;

const char* const cc::v8::basic::Evaluator::k_variable_log_func_name_ = "_log";
const char* const cc::v8::basic::Evaluator::k_variable_log_func_ =
    "function _log($) {\n"
     "    NativeLog(JSON.stringify($));\n"
    "}"
;

const char* const cc::v8::basic::Evaluator::k_variable_dump_func_name_ = "_dump";
const char* const cc::v8::basic::Evaluator::k_variable_dump_func_ =
    "function _dump(title, $) {\n"
     "    NativeDump('----- [B] ' + title + ' ------');\n"
     "    NativeDump(JSON.stringify($));\n"
     "    NativeDump('----- [E] ' + title + ' ------');\n"
    "}"
;

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data TO BE COPIED
 * @param a_owner                   Script owner.
 * @param a_name                    Script name
 * @param a_uri                     Unused.
 * @param a_out_path                Writable directory.
 * @param a_functions               Native functions to load.
 */
cc::v8::basic::Evaluator::Evaluator (const ::ev::Loggable::Data& a_loggable_data,
                                     const std::string& a_owner, const std::string& a_name, const std::string& a_uri,
                                     const std::string& a_out_path,
                                     const cc::v8::Script::NativeFunctions& a_functions)
    : ::cc::v8::Script(a_owner, a_name, a_uri, a_out_path, a_functions),
    callable_ {
        /* ctx_      */ nullptr,
        /* isolated_ */ nullptr,
        /* name_     */ "",
        /* argc_     */ 3,
        /* argv_     */ args_,
        /* where_    */ "",
        /* on_error_ */ nullptr
    },
    loggable_data_(a_loggable_data), logger_client_(nullptr)
{
    callable_.on_error_ = std::bind(&cc::v8::basic::Evaluator::FunctionCallErrorCallback, std::placeholders::_1,  std::placeholders::_2);
}

/**
 * @brief Copy constructor.
 *
 * @param a_evaluator Instance to copy.
 */
cc::v8::basic::Evaluator::Evaluator (const cc::v8::basic::Evaluator& a_evaluator)
: ::cc::v8::Script(a_evaluator.owner_, a_evaluator.name_, a_evaluator.uri_, a_evaluator.out_path_, a_evaluator.native_functions()),
    callable_ {
        /* ctx_      */ nullptr,
        /* isolated_ */ nullptr,
        /* name_     */ "",
        /* argc_     */ 3,
        /* argv_     */ args_,
        /* where_    */ "",
        /* on_error_ */ nullptr
    },
    loggable_data_(a_evaluator.loggable_data_), logger_client_(nullptr)
{
    callable_.on_error_ = std::bind(&cc::v8::basic::Evaluator::FunctionCallErrorCallback, std::placeholders::_1,  std::placeholders::_2);
}

/**
 * @brief Destructor.
 */
cc::v8::basic::Evaluator::~Evaluator ()
{
    // ... unregister from logger ...
    if ( nullptr != logger_client_ ) {
        ::ev::LoggerV2::GetInstance().Unregister(logger_client_);
        delete logger_client_;
    }
}

// MARK: -

/**
 * @brief Load this script to a specific context.
 *
 * @param a_external_scripts External scripts to load, as JSON object.
 * @param a_expressions      Expressions to load.
 */
void cc::v8::basic::Evaluator::Load (const Json::Value& a_external_scripts, const cc::v8::basic::Evaluator::Expressions& a_expressions)
{
    // ... register at logger ...
    if ( nullptr == logger_client_ ) {
        logger_client_ = new ::ev::LoggerV2::Client(loggable_data_);
        logger_client_->Unset(ev::LoggerV2::Client::LoggableFlags::IPAddress | ev::LoggerV2::Client::LoggableFlags::OwnerPTR);
        logger_token_ = name_;
        ::ev::LoggerV2::GetInstance().cc::logs::Logger::Register(logger_token_, ::cc::fs::Dir::Normalize(out_path_) + logger_token_ + ".log");
        ::ev::LoggerV2::GetInstance().Register(logger_client_, { logger_token_.c_str() });
    }
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "V8 Script");
    //
    // ... prepare script ...
    std::vector<::cc::v8::Context::Function> functions = {
        { /* name  */ k_evaluate_basic_expression_func_name_   },
        { /* name_ */ k_variable_log_func_name_                }
    };
    std::stringstream ss;
    ss.str("");
    ss << "\"use strict\";\n";
    // ... eval function ...
    ss << "\n//\n// " << k_evaluate_basic_expression_func_name_ << "\n//\n";
    ss << k_evaluate_basic_expression_func_;
    // ... log function ...
    ss << "\n\n//\n// " << k_variable_log_func_name_ << "\n//\n";
    ss << k_variable_log_func_;
    // ... dump function?
    if ( native_functions().end() != native_functions().find("NativeDump") ) {
        ss << "\n\n//\n// " << k_variable_dump_func_name_ << "\n//\n";
        ss << k_variable_dump_func_;
        functions.push_back({ /* name_ */ k_variable_dump_func_name_ });
    }
    // ... load subclass script(s) ...
    InnerLoad(a_external_scripts, a_expressions, ss);
    // ... finalize script ...
    std::string loaded_script = ss.str();
    if ( '\n' == loaded_script[loaded_script.length() - 1] ) {
        loaded_script.pop_back();
    }
    // ... keep track of this instance ...
    SetIsolateData(/* slot */ 0,  this);
    // ... compile script ...
    IsolatedCall([this, &loaded_script, &functions]
                 (::v8::Local<::v8::Context>& /* a_context */, ::v8::TryCatch& /* a_try_catch */, ::v8::Isolate* a_isolate) {
                     const ::v8::Local<::v8::String>                script    = ::v8::String::NewFromUtf8(a_isolate, loaded_script.c_str(), ::v8::NewStringType::kNormal).ToLocalChecked();
                    try {
                        // ... log ...
                        ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), CC_LOGS_LOGGER_COLOR(CYAN) "%s" CC_LOGS_LOGGER_RESET_ATTRS, "Compiling...");
                        // ... compile ...
                        std::string compiled_script;
                        Compile(script, &functions, &compiled_script);
                        // ... log ...
                        ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), CC_LOGS_LOGGER_COLOR(DARK_GRAY) "\n%s" CC_LOGS_LOGGER_RESET_ATTRS, compiled_script.c_str());
                        ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), "%s", CC_LOGS_LOGGER_COLOR(GREEN) "Compiled." CC_LOGS_LOGGER_RESET_ATTRS);
                    } catch (const cc::v8::Exception& a_v8_exception) {
                        // ... log ...
                        ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), CC_LOGS_LOGGER_COLOR(DARK_GRAY) "\n%s" CC_LOGS_LOGGER_RESET_ATTRS, loaded_script.c_str());
                        ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), CC_LOGS_LOGGER_COLOR(RED) "\n%s" CC_LOGS_LOGGER_RESET_ATTRS, a_v8_exception.what());
                        ::ev::LoggerV2::GetInstance().Log(logger_client(), logger_token(), CC_LOGS_LOGGER_COLOR(RED) "%s" CC_LOGS_LOGGER_RESET_ATTRS, "Failed.");
                        // ... re-throw ...
                        throw a_v8_exception;
                    }
                }
    );
}

// MARK: -

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

// MARK: -

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
    try {
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
    } catch (const ::cc::v8::Exception& a_v8_exception) {
        if ( nullptr != log_callback_ ) {
            log_callback_(a_v8_exception.what(), /* a_success */ false);
        }
        throw a_v8_exception;
    } catch (...) {
        try {
            ::cc::v8::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::v8::Exception& a_v8_exception) {
            if ( nullptr != log_callback_ ) {
                log_callback_(a_v8_exception.what(), /* a_success */ false);
            }
            throw a_v8_exception;
        }
    }
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
                         /* isolate_  */ a_isolate,
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

// MARK: -

/**
 * @brief The callback that is invoked by v8 whenever the JavaScript 'NativeLog' function is called.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void cc::v8::basic::Evaluator::NativeLog (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
{
    std::stringstream ss;
    if ( 0 == a_args.Length() ) {
        return;
    }
    const ::v8::HandleScope handle_scope(a_args.GetIsolate());
    const cc::v8::basic::Evaluator* self = (cc::v8::basic::Evaluator*)handle_scope.GetIsolate()->GetData(0);
    if ( nullptr == self->logger_client_ ) {
        return;
    }
    if ( a_args.Length() > 0 ) {
        ss << ToString(a_args[0], a_args.GetIsolate()->GetCurrentContext(), a_args.GetIsolate());
        for ( int i = 1 ; i < a_args.Length() ; i++ ) {
            ss << ", " << ToString(a_args[i], a_args.GetIsolate()->GetCurrentContext(), a_args.GetIsolate());
        }
    }
    if ( nullptr != self->log_callback_ ) {
        self->log_callback_(ss.str(), /* a_success */ true);
    } else {
        ::ev::LoggerV2::GetInstance().Log(self->logger_client_, self->logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(DARK_GRAY) "%s\n" CC_LOGS_LOGGER_RESET_ATTRS, ss.str().c_str());
    }
}

// MARK: -

/**
 * @brief The callback that is invoked by v8 whenever the JavaScript 'NativeLog' function is called.
 *
 * @param a_args V8 arguments ( including function args ).
 */
void cc::v8::basic::Evaluator::NativeDump (const ::v8::FunctionCallbackInfo<::v8::Value>& a_args)
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

// MARK: -

/**
 * @brief The callback that is invoked when a v8 call has failed.
 *
 * @param a_callable
 * @param a_message
 */
void cc::v8::basic::Evaluator::FunctionCallErrorCallback (const ::cc::v8::Context::LoadedFunction::Callable& a_callable, const char* const a_message)
{
    const cc::v8::basic::Evaluator* self;
    if ( nullptr != a_callable.isolate_ ) {
        const ::v8::HandleScope handle_scope(a_callable.isolate_);
        self = (cc::v8::basic::Evaluator*)handle_scope.GetIsolate()->GetData(0);
        if ( nullptr == self->logger_client_ ) {
            self = nullptr;
        }
    } else {
        self = nullptr;
    }
    std::stringstream ss;
    ss << "---->\n";
    ss << "---- WARNING ----\n";
    ss << "---- When calling:\n";
    ss << "---- ---- function: " << a_callable.name_ << "\n";
    ss << "---- ---- argc    : " << a_callable.argc_ << "\n";
    ss << a_message << "\n";
    ss << "<----\n";
    if ( nullptr != self ) {
        if ( nullptr != self->log_callback_ ) {
            self->log_callback_(ss.str(), /* a_success */ true);
        } else {
            ::ev::LoggerV2::GetInstance().Log(self->logger_client_, self->logger_token_.c_str(), CC_LOGS_LOGGER_COLOR(RED) "%s\n" CC_LOGS_LOGGER_RESET_ATTRS, ss.str().c_str());
        }
    } else {
        fprintf(stderr, "%s", ss.str().c_str());
        fflush(stderr);
    }
}

// MARK:

/**
 * @brief Translate a \link ::v8::Value \link to a \link ::v8::String::Utf8Value \link.
 *
 * @param a_value   Value to translate.
 * @param a_context V8 local context.
 * @param a_isolate V8 isolate pointer.
 *
 * @return A std::string.
 */
std::string cc::v8::basic::Evaluator::ToString (const ::v8::Local<::v8::Value>& a_value,
                                                ::v8::Local<::v8::Context> a_context, ::v8::Isolate* a_isolate)
{
    // ... no special handing?
    if ( false == a_value->IsObject() ) {
        // ... done ...
        return *::v8::String::Utf8Value(a_isolate, a_value);
    }
    // ... stringify it ..
    ::v8::Local<::v8::String> string;
    if ( false == ::v8::JSON::Stringify(a_context, a_value).ToLocal(&string) ) {
        throw cc::v8::Exception("An error ocurred while translating a V8 object to a JSON object': %s!",
                                "can't convert to a 'local' string"
        );
    }
    // ... done ...
    return * ::v8::String::Utf8Value(a_isolate, string);
}
