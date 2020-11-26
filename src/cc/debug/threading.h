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
#ifndef NRS_CC_DEBUG_THREADING_H_
#define NRS_CC_DEBUG_THREADING_H_

#include "cc/singleton.h"

#if !defined(__APPLE__)
    #include <unistd.h>
    #include <sys/syscall.h> // syscall
#else
    #include <pthread.h> // pthread_threadid_np
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
        class Threading final : public cc::Singleton<Threading, ThreadingOneShotInitializer>
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
                #ifdef CC_DEBUG_ON
                    const int rv = pthread_threadid_np(NULL, &thread_id);
                    assert(0 == rv);
                #else
                    (void)pthread_threadid_np(NULL, &thread_id);
                #endif
                return thread_id;
            #else
                return (uint64_t)syscall(SYS_gettid);
            #endif
        }
    
    }
}

#endif // NRS_CC_DEBUG_THREADING_H_
