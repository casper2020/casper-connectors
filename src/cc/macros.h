/**
 * @file macros.h
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
#ifndef NRS_CC_MACROS_H_
#define NRS_CC_MACROS_H_

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG)
    #define CC_DEBUG_ON 1
#else
    #undef CC_DEBUG_ON
#endif

#include <assert.h>

#define CC_DO_PRAGMA(x) _Pragma (#x)
#define CC_MACRO_DEFER(M,...) M(__VA_ARGS__)
#define CC_MACRO_STRINGIFY_ARG(a) #a

#define CC_SILENCE_UNUSED_VARIABLE(a_name) \
    (void)a_name

#define CC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

#define CC_WARNING_UNUSED_VARIABLE(a_name) \
  _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("TODO 2.0: unused variable '" #a_name "'"))); \
  (void)a_name;

#define CC_WARNING_TODO(a_name) \
   _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("" #a_name)))

#define CC_DEPRECATED \
    [[deprecated]]

#define CC_MARK_INTENDED_VIRTUAL_OVERRIDING(function) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Woverloaded-virtual\"") \
    function \
    _Pragma("clang diagnostic pop")\

#define CC_ASSERT(a_condition) assert(a_condition)

#include <cxxabi.h>
#include <memory> // std::unique_ptr
#include <string>
#define CC_DEMANGLE(a_name)[]() -> std::string { \
    int status = -4; \
    std::unique_ptr<char, void(*)(void*)> res { \
        abi::__cxa_demangle(a_name, NULL, NULL, &status), \
        std::free \
    }; \
    return std::string( (status==0) ? res.get() : a_name ); \
} ()

#if !defined(__APPLE__)
    #include <unistd.h>
    #include <sys/syscall.h>
#else
    #include <pthread.h>
#endif
#ifdef __APPLE__
    #define CC_CURRENT_THREAD_ID()[]() { \
        uint64_t thread_id; \
        (void)pthread_threadid_np(NULL, &thread_id); \
        return thread_id; \
    }()
#else
    #define CC_CURRENT_THREAD_ID()[]() { \
        return (uint64_t)syscall(SYS_gettid); \
    }()
#endif

#endif // NRS_CC_MACROS_H_
