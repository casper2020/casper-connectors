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

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG)
    #define CC_DEBUG_ON 1
#else
    #define CC_DEBUG_ON
#endif

#ifdef CC_DEBUG_ON

    #define CC_IF_DEBUG(a_code) a_code
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_debug

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name) a_type a_name
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value) a_type a_name = a_value

#else

    #define CC_IF_DEBUG(a_code)
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_release

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name)
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value)

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
        const auto s = std::chrono::steady_clock::now(); \
        a_code; \
        a_name##_.elapsed_ += CC_MEASURE_ELAPSED(s); \
    } while(0);

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
    #define CC_MEASURE_CALL(a_code, o_trace) \
        do { \
            a_code; \
        } while(0);
    #define CC_MEASURE_COLLECT_CALL(a_code, o_trace) \
        do { \
            a_code; \
        } while(0);

#endif

#define CC_DO_PRAGMA(x) _Pragma (#x)
#define CC_MACRO_DEFER(M,...) M(__VA_ARGS__)
#define CC_MACRO_STRINGIFY_ARG(a) #a

#define CC_WARNING_UNUSED_VARIABLE(a_name) \
  _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("TODO 2.0: unused variable '" #a_name "'"))); \
  (void)a_name;

#define CC_WARNING_TODO(a_name) \
   _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("" #a_name)))

#endif // NRS_CC_DEBUG_TYPES_H_
