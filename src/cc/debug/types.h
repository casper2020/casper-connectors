/**
 * @file types.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_DEBUG_TYPES_H_
#define NRS_CC_DEBUG_TYPES_H_

#include <chrono>

#include <pthread.h>
#include <assert.h>

#include "cc/macros.h"

#include "cc/debug/threading.h"

#if !defined(__APPLE__)
    #include <unistd.h>
    #include <sys/syscall.h>
#endif

#include "cc/debug/logger.h"

#ifdef CC_DEBUG_ON

    // DEBUG MODE

// #if !defined(NDEBUG)
    #define CC_DEBUG_ASSERT(a_condition)  \
        ((void) ((a_condition) ? ((void)0) : __CC_DEBUG_ASSERT (#a_condition, __FILE__, __LINE__)))
    #define __CC_DEBUG_ASSERT(a_condition, a_file, a_line) \
        ((void)printf("[CC_DEBUG] ⚠️ @ %s:%d: failed assertion `%s'\n", a_file, a_line, a_condition), abort())
//#else
//    #define CC_DEBUG_ASSERT(a_condition)  \
//        assert(a_condition)
//#endif

    #define CC_DEBUG_ABORT() \
        CC_DEBUG_ASSERT(1==2)

    #define CC_DEBUG_SET_MAIN_THREAD_ID() \
        cc::debug::Threading::GetInstance().Start();

    #define CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD() \
        CC_DEBUG_ASSERT(true == cc::debug::Threading::GetInstance().AtMainThread())

    #define CC_DEBUG_FAIL_IF_NOT_AT_THREAD(a_id) \
        CC_DEBUG_ASSERT(cc::debug::Threading::GetInstance().CurrentThreadID() == a_id)

    #define CC_IF_DEBUG(...) __VA_ARGS__
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_debug

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name) a_type a_name
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value) a_type a_name = a_value
    #define CC_IF_DEBUG_SET_VAR(a_name, a_value) a_name = a_value

    #define CC_IF_DEBUG_CONSTRUCT_DECLARE_VAR(a_type, a_name, ...) a_type a_name __VA_ARGS__
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(a_type, a_name) , a_type a_name
    #define CC_IF_DEBUG_CONSTRUCT_SET_VAR(a_name, a_value, ...) a_name(a_value) __VA_ARGS__
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_SET_VAR(a_name, a_value, ...) , a_name(a_value) __VA_ARGS__
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(a_value) , a_value

    #define CC_IF_DEBUG_FUNCTION_SET_PARAM_VALUE(a_value, ...) a_value __VA_ARGS__

    #define CC_IF_DEBUG_LAMBDA_CAPTURE(...) __VA_ARGS__

    #define CC_DEBUG_LOG_ENABLE(a_token) \
        ::cc::debug::Logger::GetInstance().Register(a_token);

    #define CC_DEBUG_LOG_MSG(a_token, a_format, ...) \
        ::cc::debug::Logger::GetInstance().Log(a_token, "[%s] " a_format "\n", a_token, __VA_ARGS__);

    #define CC_DEBUG_LOG_PRINT(a_token, a_format, ...) \
        ::cc::debug::Logger::GetInstance().Log(a_token, a_format, __VA_ARGS__);

    #define CC_DEBUG_LOG_TRACE(a_token, a_format, ...) \
        ::cc::debug::Logger::GetInstance().Log(a_token, "\n[%s] @ %-4s:%4d\n\n\t* " a_format "\n",  a_token, __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);

    #define CC_DEBUG_LOG_RECYCLE() \
        ::cc::debug::Logger::GetInstance().Recycle()

    #define CC_DEBUG_LOG_IF_REGISTERED_RUN(a_token, ...) \
        if ( true == ::cc::debug::Logger::GetInstance().IsRegistered(a_token) ) { \
            __VA_ARGS__ \
        }

#else

    // RELEASE MODE

    #define CC_DEBUG_ASSERT(a_condition)
    #define CC_DEBUG_ABORT()

    #define CC_DEBUG_SET_MAIN_THREAD_ID()
    #define CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD()
    #define CC_DEBUG_FAIL_IF_NOT_AT_THREAD(a_id)

    #define CC_IF_DEBUG(...)
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_release

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name)
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value)
    #define CC_IF_DEBUG_SET_VAR(a_name, a_value)

    #define CC_IF_DEBUG_CONSTRUCT_DECLARE_VAR(a_type, a_name, ...)
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(a_type, a_name)
    #define CC_IF_DEBUG_CONSTRUCT_SET_VAR(a_name, a_value, ...)
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_SET_VAR(a_name, a_value, ...)
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(a_value)

    #define CC_IF_DEBUG_FUNCTION_SET_PARAM_VALUE(a_value, ...)

    #define CC_IF_DEBUG_LAMBDA_CAPTURE(...)

    #define CC_DEBUG_LOG_ENABLE(a_token)
    #define CC_DEBUG_LOG_PRINT(a_token, a_format, ...)
    #define CC_DEBUG_LOG_MSG(a_token, a_format, ...)
    #define CC_DEBUG_LOG_TRACE(a_token, a_format, ...)
    #define CC_DEBUG_LOG_RECYCLE()
    #define CC_DEBUG_LOG_IF_REGISTERED_RUN(a_token, ...)

#endif

    #define CC_MEASURE_ELAPSED(a_start_tp) \
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - a_start_tp).count());
    #define CC_MEASURE_UNITS "ms" // "µs"

    #define CC_CLASS_NAME(a_ptr) [] () -> std::string { \
        std::string tmp = CC_DEMANGLE(typeid(a_ptr).name()); \
        tmp.pop_back(); \
        const size_t p = tmp.rfind("::"); \
        if ( std::string::npos != p ) { \
            return std::string(tmp.c_str() + p + ( sizeof(char) * 2 ) ); \
        } \
        return tmp;\
    } ()

    #define CC_QUALIFIED_CLASS_NAME(a_ptr) [] () -> std::string { \
        std::string tmp = CC_DEMANGLE(typeid(a_ptr).name()); \
        tmp.pop_back(); \
        return tmp; \
    } ()

#if 1

    typedef struct {
        uint64_t elapsed_;
    } BlockTrace;

    #define CC_IF_MEASURE(a_code) \
        do { \
            a_code; \
        } while(0);

    #define CC_MEASURE_DECLARE(a_name) BlockTrace a_name##_ = { /* elapsed_ */ 0 }
    #define CC_MEASURE_RESET(a_name) a_name##_ = { /* elapsed_ */ 0 }
    #define CC_MEASURE_GET(a_name) \
        a_name##_.elapsed_
    #define CC_MEASURE_DECLARE_ACCESSOR(a_name) inline const BlockTrace& cc_trace_get_##a_name () const { return a_name##_; }

    #define CC_MEASURE_CALL(a_code, a_name) \
    do { \
        a_name##_.elapsed_ = 0; \
        const auto a_name##_sp = std::chrono::steady_clock::now(); \
        a_code; \
        a_name##_.elapsed_ = CC_MEASURE_ELAPSED(a_name##_sp); \
    } while(0);

    #define CC_MEASURE_CALLBACK(a_code, a_name)[&]() { \
        a_name##_.elapsed_ = 0; \
        const auto a_name##_sp = std::chrono::steady_clock::now(); \
        a_code; \
        a_name##_.elapsed_ = CC_MEASURE_ELAPSED(a_name##_sp); \
    } ()

    #define CC_MEASURE_COLLECT_CALL(a_code, a_name) \
    do { \
        const auto s = std::chrono::steady_clock::now(); \
        a_code; \
        a_name##_.elapsed_ += CC_MEASURE_ELAPSED(s); \
    } while(0);

#else

    #define CC_IF_MEASURE(a_code)
    #define CC_MEASURE_DECLARE(a_type)
    #define CC_MEASURE_RESET(a_name)
    #define CC_MEASURE_GET(a_name)
    #define CC_MEASURE_CALLBACK(a_code, a_name) \
        a_code;
    #define CC_MEASURE_CALL(a_code, o_trace) \
        do { \
            a_code; \
        } while(0);
    #define CC_MEASURE_COLLECT_CALL(a_code, o_trace) \
        do { \
            a_code; \
        } while(0);

#endif

#endif // NRS_CC_DEBUG_TYPES_H_
