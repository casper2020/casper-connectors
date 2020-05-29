/**
 * @file types.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_DEBUG_TYPES_H_
#define NRS_CC_DEBUG_TYPES_H_

#include <chrono>

#include "cc/debug/logger.h"

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG)
    #define CC_DEBUG_ON 1
#else
    #undef CC_DEBUG_ON
#endif

#ifdef CC_DEBUG_ON

    // DEBUG MODE

    #define CC_IF_DEBUG(a_code) a_code
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_debug

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name) a_type a_name
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value) a_type a_name = a_value

    #define CC_DEBUG_LOG_ENABLE(a_token) \
        ::cc::debug::Logger::GetInstance().Register(a_token);

    #define CC_DEBUG_LOG_MSG(a_token, a_format, ...) \
        ::cc::debug::Logger::GetInstance().Log(a_token, "[%s] " a_format "\n", a_token, __VA_ARGS__);

    #define CC_DEBUG_LOG_TRACE(a_token, a_format, ...) \
        ::cc::debug::Logger::GetInstance().Log(a_token, "\n[%s] @ %-4s:%4d\n\n\t* " a_format "\n",  a_token, __PRETTY_FUNCTION__, __LINE__, __VA_ARGS__);

    #define CC_DEBUG_LOG_RECYCLE() \
        ::cc::debug::Logger::GetInstance().Recycle()

    #define CC_DEBUG_LOG_IF_REGISTERED_RUN(a_token, a_code) \
        if ( true == ::cc::debug::Logger::GetInstance().IsRegistered(a_token) ) { \
            a_code; \
        }

#else

    // RELEASE MODE

    #define CC_IF_DEBUG(a_code)
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_release

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name)
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value)

    #define CC_DEBUG_LOG_ENABLE(a_token)
    #define CC_DEBUG_LOG_MSG(a_token, a_format, ...)
    #define CC_DEBUG_LOG_TRACE(a_token, a_format, ...)
    #define CC_DEBUG_LOG_RECYCLE()
    #define CC_DEBUG_LOG_IF_REGISTERED_RUN(a_token, a_code)

#endif

    #define CC_MEASURE_ELAPSED(a_start_tp) \
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - a_start_tp).count());
    #define CC_MEASURE_UNITS "ms" // "µs"

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
    #define CC_MEASURE_CALLBACK(a_callback, a_name) \
        a_callback();
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
