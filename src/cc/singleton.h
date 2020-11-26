/**
 * @file singleton.h - NOT Thread safe singleton
 *
 * Based on code originally developed for NDrive S.A.
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-osal.
 *
 * casper-osal is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-osal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with osal.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CASPER_SINGLETON_H_
#define CASPER_SINGLETON_H_
#pragma once

#include <type_traits>

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/macros.h"
#include "cc/types.h"

#if 0 && defined(CC_DEBUG_ON)
    #include <stdio.h>  // vsnprintf
    #include <stdarg.h> // va_list, va_start, va_arg, va_end
    #include <typeinfo> // typeid

    #define CC_SINGLETON_DEBUG_TRACE(a_format, ...) \
        do { \
            fprintf(stdout, a_format, __VA_ARGS__); \
            fflush(stdout); \
        } while (0)
#else
    #define CC_SINGLETON_DEBUG_TRACE(a_format, ...)
#endif

namespace cc
{

    // -- -- //

    template <typename C> class Initializer
    {

    protected: // Refs

        C& instance_;

    public: // Constructor(s) / Destructor

        Initializer (C& a_instance);
        virtual ~Initializer ();

    };
    
    template <typename C> Initializer<C>::Initializer (C& a_instance)
        : instance_(a_instance)
    {
        CC_SINGLETON_DEBUG_TRACE("\t⌁ [%p @ " UINT64_FMT_LP(8) " ✔︎] : %s\n", this, CC_CURRENT_THREAD_ID(), CC_DEMANGLE(typeid(this).name()).c_str());
    }

    template <typename C> Initializer<C>::~Initializer ()
    {
        CC_SINGLETON_DEBUG_TRACE("\t⌁ [%p @ " UINT64_FMT_LP(8) " ✗] : %s\n", this, CC_CURRENT_THREAD_ID(), CC_DEMANGLE(typeid(this).name()).c_str());
    }

    // -- -- //

    template <typename T, typename I, typename = std::enable_if<std::is_base_of<Initializer<T>, I>::value>> class Singleton : public NonCopyable, public NonMovable
    {

    public: // Constructor(s))
        
        Singleton (const Singleton<T,I>&) = delete; // Copy constructor
        Singleton (Singleton<T,I>&&)      = delete; // Move constructor
        
    protected: // Constructor // Destructor
        
        Singleton ()
        {
            CC_SINGLETON_DEBUG_TRACE("⌁ [%p @ " UINT64_FMT_LP(8) " ✔︎] : %s\n", this, CC_CURRENT_THREAD_ID(), CC_DEMANGLE(typeid(T).name()).c_str());
        }
                
        ~Singleton()
        {
            CC_SINGLETON_DEBUG_TRACE("⌁ [%p @ " UINT64_FMT_LP(8) " ✗] : %s\n", this, CC_CURRENT_THREAD_ID(), CC_DEMANGLE(typeid(T).name()).c_str());
        }
        
    public: // Operator(s) Overload
        
        Singleton<T,I>& operator=(Singleton<T,I> &&)                   = delete; // Move assign
        Singleton<T,I>& operator = (const Singleton<T,I>& a_singleton) = delete; // Copy assign
        
    public: // inline method(s) / function(s) declaration
        
        static T& GetInstance () __attribute__ ((visibility ("default")))
        {
            // C++11 'magic statics' aproatch
            static T t;
            static I i(t);
            return t;
        }
        
    }; // end of template Singleton

} // end of namespace casper

#endif // CASPER_SINGLETON_H_
