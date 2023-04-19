/**
 * @file macros.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_MACROS_H_
#define NRS_CC_MACROS_H_

#include "cc/config.h" // pulling CC_DEBUG_ON

#include <assert.h>

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
