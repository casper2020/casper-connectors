/**
 * @file compiler_defs.h
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_COMPILER_DEFS_H_
#define NRS_CC_COMPILER_DEFS_H_

// COMPILER
#if defined(__clang__)
    #define CC_USING_CLANG 1
#elif defined(__GNUC__)
    #define CC_USING_GCC 1
#else
    #error Don\'t know which compiler!
#endif

// C++ VERSION
#if __cplusplus >=202002L
    #define CC_CPP_VERSION 20L
#elif __cplusplus >=201703L
    #define CC_CPP_VERSION 17L
#elif __cplusplus >=201402L
    #define CC_CPP_VERSION 14L
#elif __cplusplus >= 201103L
    #define CC_CPP_VERSION 11L
#elif __cplusplus < 201103L
    #error This project needs at least a C++11 compliant compiler
#endif

// PRAGMAS

#define CC_DO_PRAGMA(x) _Pragma (#x)
#define CC_MACRO_DEFER(M,...) M(__VA_ARGS__)
#define CC_MACRO_STRINGIFY_ARG(a) #a

#if CC_CPP_VERSION >= 17L
    #define CC_DECLARE_UNUSED_VARIABLE(a_type, a_name) [[maybe_unused]]a_type a_name
#else
    #define CC_DECLARE_UNUSED_VARIABLE(a_type, a_name) a_type a_name __attribute__((unused))
#endif

#define CC_SILENCE_UNUSED_VARIABLE(a_name) (void)a_name

#define CC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

#define CC_WARNING_UNUSED_VARIABLE(a_name) \
  _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("TODO 2.0: unused variable '" #a_name "'"))); \
  (void)a_name;

#define CC_WARNING_TODO(a_message) \
    CC_DO_PRAGMA(message ("WARNING TODO: " #a_message))

#define CC_DEPRECATED \
    [[deprecated]]

#if (defined(__clang__) && !defined(__GNUC__))
    #define CC_USING_CLANG 1
#endif

// DIAGNOSTIC

#if 1 == CC_USING_CLANG
    #define CC_DIAGNOSTIC_PUSH() \
        CC_DO_PRAGMA(clang diagnostic push)
    #define CC_DIAGNOSTIC_IGNORED(warning) \
        CC_DO_PRAGMA(clang diagnostic ignored warning)
    #define CC_DIAGNOSTIC_POP() \
        CC_DO_PRAGMA(clang diagnostic pop)
    #define CC_MARK_INTENDED_VIRTUAL_OVERRIDING(function) \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Woverloaded-virtual\"") \
        function \
        _Pragma("clang diagnostic pop")
#elif 1 == CC_USING_GCC
    #define CC_DIAGNOSTIC_PUSH() \
        CC_DO_PRAGMA(GCC diagnostic push);
    #define CC_DIAGNOSTIC_IGNORED(warning) \
        CC_DO_PRAGMA(GCC diagnostic ignored warning)
    #define CC_DIAGNOSTIC_POP() \
        CC_DO_PRAGMA(GCC diagnostic pop)
    #define CC_MARK_INTENDED_VIRTUAL_OVERRIDING(function) \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Woverloaded-virtual\"") \
        function \
        _Pragma("GCC diagnostic pop")
#else
    #error Don\'t know which compiler!
#endif

#endif // NRS_CC_COMPILER_DEFS_H_
