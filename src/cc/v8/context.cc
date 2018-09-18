/**
 * @file context.cc
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

#include "cc/v8/context.h"

#include "cc/v8/exception.h"

/**
 * @brief Default constructor.
 *
 * @brief https://github.com/v8/v8/wiki/Embedder%27s-Guide
 *
 * @param a_isolate_ptr
 * @param a_functions
 */
cc::v8::Context::Context (::v8::Isolate* a_isolate_ptr, const cc::v8::Context::NativeFunctions& a_functions)
    : isolate_ptr_(a_isolate_ptr)
{
    // ... prepare context ...
    ::v8::HandleScope handle_scope(isolate_ptr_);
    // add debug function
    ::v8::Local<::v8::ObjectTemplate> global = ::v8::ObjectTemplate::New(isolate_ptr_);
    // ... load global functions ...
    for ( auto it : a_functions ) {
        global->Set(::v8::String::NewFromUtf8(isolate_ptr_,
                                              it.first.c_str(),
                                              ::v8::NewStringType::kNormal).ToLocalChecked(),
                                              ::v8::FunctionTemplate::New(isolate_ptr_, it.second)
        );
    }
    // ... make it eternal ...
    context_.Set(isolate_ptr_, ::v8::Context::New(isolate_ptr_, NULL, global));
}

/**
 * @brief Destructor constructor.
 */
cc::v8::Context::~Context ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Parse a JSON string into an \link ::v8::Object \link.
 *
 * @param a_uri
 * @param o_object
 * @param o_data
 *
 * @return
 */
bool cc::v8::Context::Parse (const std::string& a_uri,
                                 ::v8::Persistent<::v8::Object>& o_object, ::v8::Persistent<::v8::Value>& o_data) const
{
    std::ifstream ifs(a_uri);
    std::string   data((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    
    return Parse(data.c_str(), o_object, o_data);
}

/**
 * @brief Parse a JSON string into an \link ::v8::Object \link.
 *
 * @param a_data
 * @param o_object
 * @param o_data
 *
 * @return
 */
bool cc::v8::Context::Parse (const char* const a_data,
                                 ::v8::Persistent<::v8::Object>& o_object, ::v8::Persistent<::v8::Value>& o_data) const
{
    // set up an error handler to catch any exceptions the script might throw.
    ::v8::TryCatch try_catch(isolate_ptr_);
    try_catch.SetVerbose(true);
    
    ::v8::Local<::v8::Context> context = context_.Get(isolate_ptr_);
    
    ::v8::Context::Scope       context_scope(context);
    ::v8::HandleScope          handle_scope (isolate_ptr_);
    
    const ::v8::Local<::v8::String> payload = ::v8::String::NewFromUtf8(isolate_ptr_, a_data, ::v8::NewStringType::kNormal).ToLocalChecked();
    
    CASPER_V8_CHRONO_START(parse);
    ::v8::MaybeLocal<::v8::Value> value = ::v8::JSON::Parse(context, payload);
    CASPER_V8_CHRONO_END(parse, "parse %zd byte(s) of JSON data", strlen(a_data));
    
    const ::v8::String::Utf8Value exception(try_catch.Exception());
    
    if ( exception.length() == 0 ) {
        o_object.Reset(isolate_ptr_, value.ToLocalChecked()->ToObject(context).ToLocalChecked());
        o_data.Reset(isolate_ptr_, value.ToLocalChecked());
    } else {
        o_object.Reset();
        o_data.Reset();
    }
    
    return ( false == o_object.IsEmpty() );
}

/**
 * @brief Compile a script and ( optionaly ) load some functions from it.
 *
 * @param a_name
 * @param a_script
 * @param a_functions
 * @param a_on_error
 */
void cc::v8::Context::Compile (const std::string& a_name,
                                   const ::v8::Local<::v8::String>& a_script,
                                   const cc::v8::Context::FunctionsVector* a_functions)
{
    // TODO v8 - use callable
    // set up an error handler to catch any exceptions the script might throw.
    ::v8::TryCatch try_catch(isolate_ptr_);
    
    // TODO 2.0 v8 - READ THIS AND DOUBLE CHECK CODE
    // Each processor gets its own context so different processors don't
    // affect each other. Context::New returns a persistent handle which
    // is what we need for the reference to remain after we return from
    // this method. That persistent handle has to be disposed in the
    // destructor.
    
    ::v8::Local<::v8::Context> context = context_.Get(isolate_ptr_);
    
    ::v8::Context::Scope       context_scope(context);
    ::v8::HandleScope          handle_scope (isolate_ptr_);
    
    ::v8::String::Utf8Value    script(a_script);
    
    //
    // Compile the script and check for errors.
    //
    ::v8::Local<::v8::Script> local_compiled_script;
    if ( false == ::v8::Script::Compile(context, a_script).ToLocal(&local_compiled_script) ) {
        //
        // the script failed to compile
        //
        // ... report it ...
        ThrowException(&try_catch);
    }
    // make it permanent
    script_.Set(isolate_ptr_, local_compiled_script);
    
    //
    // Run the script
    //
    ::v8::Local<::v8::Value> result;
    if ( false == script_.Get(isolate_ptr_)->Run(context).ToLocal(&result) ) {
        //
        // the script failed to run
        //
        // ... report it ...
        ThrowException(&try_catch);
    }
    
    //
    // Load functions?
    //
    if ( nullptr != a_functions ) {
        LoadFunctions(context, context_scope, try_catch, *a_functions, functions_);
    }
}

/**
 * @brief Load some functions previously compiled.
 *
 * @param a_callble
 * @param a_functions
 */
void cc::v8::Context::LoadFunctions (const cc::v8::Context::LoadedFunction::Callable& a_callable,  const cc::v8::Context::FunctionsVector& a_functions)
{
    // set up an error handler to catch any exceptions the script might throw.
    ::v8::TryCatch try_catch(isolate_ptr_);
    
    // Create a handle scope to keep the temporary object references.
    ::v8::HandleScope handle_scope(isolate_ptr_);
    
    // Create a new local context
    ::v8::Local<::v8::Context> context = context_.Get(isolate_ptr_);
    
    // Enter this function's context so all the remaining operations take place there
    ::v8::Context::Scope context_scope(context);
    
    //
    // Load functions
    //
    LoadFunctions(context, context_scope, try_catch, a_functions, functions_);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Execute a previously load function.
 *
 * @param a_callable
 *
 * @param o_result
 *
 * @return
 */
void cc::v8::Context::CallFunction (const cc::v8::Context::LoadedFunction::Callable& a_callable, ::v8::Persistent<::v8::Value>& o_result) const
{
    // pick function info
    const auto it = functions_.find(a_callable.name_);
    if ( functions_.end() == it ) {
        throw cc::v8::Exception("Error while calling function '%s' - not registered!", a_callable.name_);
    }

    // set up an error handler to catch any exceptions the script might throw.
    ::v8::TryCatch try_catch(isolate_ptr_);

    // grab function
    ::v8::Local<::v8::Function> local_function = it->second->f_.Get(isolate_ptr_);
    
    // call function
    ::v8::Local<::v8::Value> local_result;
    const bool rv = local_function->Call(*a_callable.ctx_ , (*a_callable.ctx_)->Global(), a_callable.argc_, a_callable.argv_).ToLocal(&local_result);
    if ( false == rv ) {
        ThrowException(&try_catch);
    } else {
        o_result.Reset(isolate_ptr_, local_result);
    }
}

/**
 * @brief Prepare scope for a v8 context.
 *
 * @param a_callback
 */
void cc::v8::Context::IsolatedCall (const cc::v8::Context::IsolatedCallback a_callback) const
{
    // ... sanity checkpoint ...
    if ( nullptr == isolate_ptr_ ) {
        throw cc::v8::Exception("%s", "Error while preparing isolated call - nullptr!");
    }
    // ... set up an error handler to catch any exceptions the script might throw ...
    ::v8::TryCatch try_catch(isolate_ptr_);
    //
    ::v8::Isolate::Scope isolate_scope(isolate_ptr_);
    // ... create a handle scope to keep the temporary object references ...
    ::v8::HandleScope handle_scope(isolate_ptr_);
    
    // ... create a new local context ...
    ::v8::Local<::v8::Context> context = context_.Get(isolate_ptr_);
    
    // ... enter this function's context so all the remaining operations take place there ...
    ::v8::Context::Scope context_scope(context);
    
    // ... execute ...
    a_callback(context, try_catch, isolate_ptr_);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Load some functions previously compiled for a specific context and scope.
 *
 * @param a_context
 * @param a_scope
 * @param a_try_catch
 * @param a_functions
 * @param o_functions
 */
void cc::v8::Context::LoadFunctions (::v8::Local<::v8::Context>& a_context, ::v8::Context::Scope& a_scope, ::v8::TryCatch& a_try_catch,
                                         const FunctionsVector& a_functions, std::map<std::string, LoadedFunction*>& o_functions)
{
    // catch any exceptions the script might throw.
    ::v8::TryCatch try_catch(a_context->GetIsolate());
    
    for ( auto it : a_functions ) {
        
        const ::v8::Local<::v8::String> func_name = ::v8::String::NewFromUtf8(a_context->GetIsolate(), it.name_, ::v8::NewStringType::kNormal).ToLocalChecked();
        ::v8::Local<::v8::Value>  process_val;
        // If there is no function, or if it is not a function, bail out
        if ( ! a_context->Global()->Get(a_context, func_name).ToLocal(&process_val) || ! process_val->IsFunction() ) {
            // failed to load function - bail out
            throw cc::v8::Exception("Unable to load function '%s'!", it.name_);
        }
        // It is a function; cast it to a Function
        const ::v8::Local<::v8::Function> process_fun = ::v8::Local<::v8::Function>::Cast(process_val);
        
        // create a new \link LoadedFunction \link
        LoadedFunction* function = new LoadedFunction(it.name_);
        
        // keep track of new funcion
        o_functions[it.name_] = function;
        
        // store the function in a Global handle,
        // since we also want that to remain after this call returns
        function->f_.Set(a_context->GetIsolate(), process_fun);
        
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Collect all available data about an exception and return it as a string.
 *
 * @param a_try_catch
 * @param a_on_error
 */
bool cc::v8::Context::TraceException (::v8::TryCatch* a_try_catch, std::string& o_trace) const
{
    ::v8::HandleScope             handle_scope(isolate_ptr_);
    const ::v8::String::Utf8Value exception(a_try_catch->Exception());
    
    //
    // Data provided?
    //
    if ( 0 == exception.length() ) {
        // no - can't trace it
        return false;
    }
    
    const char* const exception_c_str = *exception;
    
    //
    // Message provided?
    //
    const ::v8::Local<::v8::Message> message = a_try_catch->Message();
    if ( true == message.IsEmpty() ) {
        //
        // no, v8 DIDN'T provide any extra information about this error - just print the exception
        //
        o_trace = exception_c_str;
        return true;
    }
    
    std::stringstream ss;
    
    ss.str("");
    
    //
    // v8 DID provide any extra information about this error - collect all available info
    //
    const ::v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());

    // grab context
    ::v8::Local<::v8::Context> context = context_.Get(isolate_ptr_);
    
    // <filename>:<line number>: <message>
    ss << *filename << ':' << message->GetLineNumber(context).FromJust() << ": " << exception_c_str << '\n';
    
    // line of source code.
    const ::v8::String::Utf8Value source_line(message->GetSourceLine(context).ToLocalChecked());
    
    ss << *source_line << '\n';
    
    // print wavy underline (GetUnderline is deprecated).
    const int start = message->GetStartColumn(context).FromJust();
    for ( int i = 0; i < start; i++ ) {
        ss << '~';
    }
    
    const int end = message->GetEndColumn(context).FromJust();
    for ( int i = start; i < end; i++ ) {
        ss << '^';
    }
    
    ::v8::Local<::v8::Value> stack_trace_v8_string;
    if ( a_try_catch->StackTrace(context).ToLocal(&stack_trace_v8_string)
        &&
        stack_trace_v8_string->IsString()
        &&
        ::v8::Local<::v8::String>::Cast(stack_trace_v8_string)->Length() > 0
    ) {
        ::v8::String::Utf8Value stack_trace_utf8_str(stack_trace_v8_string);
        ss << *stack_trace_utf8_str;
    }
    
    // grab string
    o_trace = ss.str();
    
    // success if string is not empty
    return ( o_trace.length() > 0 );
}

/**
 * @brief Collect all available data about an exception and log it.
 *
 * @param a_try_catch
 * @param a_on_error
 */
void cc::v8::Context::ThrowException (::v8::TryCatch* a_try_catch) const
{
    std::string trace_str;
    if ( true == cc::v8::Context::TraceException(a_try_catch, trace_str) ) {
        throw cc::v8::Exception("%s", trace_str.c_str());
    } else {
        throw cc::v8::Exception("%s", "An untraceable exception occurred while calling a function at a V8 context!\n");
    }
}
