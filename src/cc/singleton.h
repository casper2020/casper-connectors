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

// TODO 2.0
//#if ( defined(NDEBUG) && !( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) ) )
    #include <stdio.h>  // vsnprintf
    #include <stdarg.h> // va_list, va_start, va_arg, va_end
    #include <typeinfo>

    #define CC_SINGLETON_DEBUG_TRACE(a_format, ...) \
        do { \
            fprintf(stdout, a_format, __VA_ARGS__); \
            fflush(stdout); \
        } while (0)
//#else
//    #define CC_SINGLETON_DEBUG_TRACE(a_format, ...)
//#endif

namespace cc
{
    
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
        
    }
    
    template <typename C> Initializer<C>::~Initializer ()
    {
        /* empty */
    }
    
    template <typename T, typename I, typename = std::enable_if<std::is_base_of<Initializer<T>, I>::value>> class Singleton : public NonCopyable, public NonMovable
    {
        
    private: // Static Data
        
        static T* instance_;
        static I* initializer_;
        
    protected: // constructor
        
        Singleton (Singleton&) = delete;
        Singleton (Singleton&&) = delete;
        Singleton ()
        {
            /* empty */
            // CC_SINGLETON_DEBUG_TRACE("\t⌥ [%p ✔︎] : %s\n", this, typeid(T).name());
        }
                
        ~Singleton()
        {
            /* empty */
            // CC_SINGLETON_DEBUG_TRACE("\t⌥ [%p ✗] : %s\n", this, typeid(T).name());
        }
                
    private: // operators
        
        T& operator = (const T& a_singleton)
        {
            if ( &a_singleton != this ) {
                instance_ = a_singleton.instance_;
            }
            return this;
        }
        
    public: // inline method(s) / function(s) declaration
        
        static T& GetInstance () __attribute__ ((visibility ("default")))
        {
            if ( nullptr == Singleton<T,I>::instance_ ) {
                Singleton<T,I>::instance_    = new T();
                Singleton<T,I>::initializer_ = new I(*Singleton<T,I>::instance_);
            }
            return *Singleton<T,I>::instance_;
        }
        
        static void Destroy ()
        {
            if ( nullptr != Singleton<T,I>::initializer_ ) {
                delete initializer_;
                initializer_ = nullptr;
            }
            if ( nullptr != Singleton<T,I>::instance_ ) {
                delete instance_;
                instance_ = nullptr;
            }
        }
        
    }; // end of class Singleton
    
    template <typename T, typename I, typename S> T* Singleton<T,I,S>::instance_    = nullptr;
    template <typename T, typename I, typename S> I* Singleton<T,I,S>::initializer_ = nullptr;


} // end of namespace casper

#endif // CASPER_SINGLETON_H_
