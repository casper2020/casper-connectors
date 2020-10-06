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

#include "cc/singleton.h"

#if !defined(__APPLE__)
    #include <unistd.h>
    #include <sys/syscall.h>
#endif

#include "cc/debug/logger.h"

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG)
    #define CC_DEBUG_ON 1
#else
    #undef CC_DEBUG_ON
#endif

#ifdef CC_DEBUG_ON

    // DEBUG MODE

    #define CC_DEBUG_ASSERT(a_condition) \
        assert(a_condition)

    #define CC_DEBUG_SET_MAIN_THREAD_ID() \
        cc::debug::Threading::GetInstance().Start();

    #define CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD() \
        CC_DEBUG_ASSERT(true == cc::debug::Threading::GetInstance().AtMainThread())

    #define CC_DEBUG_FAIL_IF_NOT_AT_THREAD(a_id) \
        CC_DEBUG_ASSERT(cc::debug::Threading::GetInstance().CurrentThreadID() == a_id)

    #define CC_IF_DEBUG(a_code) a_code
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_debug

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name) a_type a_name
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value) a_type a_name = a_value

    #define CC_IF_DEBUG_CONSTRUCT_DECLARE_VAR(a_type, a_name, ...) a_type a_name __VA_ARGS__
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(a_type, a_name) , a_type a_name
    #define CC_IF_DEBUG_CONSTRUCT_SET_VAR(a_name, a_value, ...) a_name(a_value) __VA_ARGS__
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_SET_VAR(a_name, a_value, ...) , a_name(a_value) __VA_ARGS__

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

    #define CC_DEBUG_ASSERT(a_condition)

    #define CC_DEBUG_SET_MAIN_THREAD_ID()
    #define CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD()
    #define CC_DEBUG_FAIL_IF_NOT_AT_THREAD(a_id)

    #define CC_IF_DEBUG(a_code)
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_release

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name)
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value)

    #define CC_IF_DEBUG_CONSTRUCT_DECLARE_VAR(a_type, a_name, ...)
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_VAR(a_type, a_name)
    #define CC_IF_DEBUG_CONSTRUCT_SET_VAR(a_name, a_value, ...)
    #define CC_IF_DEBUG_CONSTRUCT_APPEND_SET_VAR(a_name, a_value, ...)

    #define CC_IF_DEBUG_CONSTRUCT_SET(a_name, a_value, ...)

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

namespace cc
{
    namespace debug
    {
    
        // ---- //
        class Threading;
        class ThreadingOneShotInitializer final : public ::cc::Initializer<Threading>
        {
          
        public: // Constructor(s) / Destructor
          
          ThreadingOneShotInitializer (::cc::debug::Threading& a_instance);
          virtual ~ThreadingOneShotInitializer ();
          
        }; // end of class 'OneShot'

        // ---- //
        class Threading final : public ::cc::logs::Logger, public cc::Singleton<Threading, ThreadingOneShotInitializer>
        {
          
          friend class ThreadingOneShotInitializer;
            
        public: // Types
            
            typedef uint64_t ThreadID;

        public: // Static Const Data
            
            static const ThreadID k_invalid_thread_id_;

        private: // Static Data
            
            ThreadID main_thread_id_;
            
        public: // Inline Method(s) / Function(s)
            
            void     Start ();
            bool     AtMainThread    () const;
            ThreadID CurrentThreadID () const;

        };
        
        /**
         * @brief Mark the current thread as the 'main' one.
         */
        inline void Threading::Start ()
        {
            main_thread_id_ = CurrentThreadID();
        }
        
        /**
         * @brief Check if this function is being called at 'main' thread.
         */
        inline bool Threading::AtMainThread () const
        {
            return ( k_invalid_thread_id_ != main_thread_id_ && CurrentThreadID() == main_thread_id_ );
        }
        
        /**
         * @return The current thread id.
         *
         * @throw An exception when it's not possible to retrieve the current thread id.
         */
        inline Threading::ThreadID Threading::CurrentThreadID () const
        {
            #ifdef __APPLE__
                uint64_t thread_id;
                const int rv = pthread_threadid_np(NULL, &thread_id);
                assert(0 == rv);
                return thread_id;
            #else
                return (uint64_t)syscall(SYS_gettid);
            #endif
        }
    
    }
}

#endif // NRS_CC_DEBUG_TYPES_H_
