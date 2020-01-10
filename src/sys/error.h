/**
 * @file error.h
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
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
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NRS_SYS_ERROR_H_
#define NRS_SYS_ERROR_H_
#pragma once

#include <string>

#ifdef __APPLE
  #include <errno.h>  // errno_t
#else
  #define errno_t int
#endif

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

namespace sys
{
    
    class Error final : public ::cc::NonCopyable, public ::cc::NonMovable
    {
        
    public: // Static Const Data
        
        static const errno_t k_no_error_ = 0;
        
    private: // Data
        
        errno_t     no_;  //!< errno
        std::string str_; //!< errno string
        std::string msg_; //!< more detailed message
        std::string fnc_; //!< function where the error occurred
        int         ln_;  //!< line number where the error occurred
        
    public: // Constructor(s) / Destructor
        
        /**
         * @brief Default constructor
         */
        Error ()
        {
            no_ = k_no_error_;
            ln_ = 0;
        }
        
        /**
         * @brief Destructor.
         */
        virtual ~Error ()
        {
            /* empty */
        }
        
    public: // Method(s) / Function(s)
        
        /**
         * @brief Reset error data.
         */
        inline void Reset ()
        {
            no_  = k_no_error_;
            str_ = "";
            msg_ = "";
            fnc_ = "";
            ln_  = 0;
        }         
        
    public: // Operator(s) Overloading
        
        inline void operator = (const errno_t a_errno)
        {
            if ( k_no_error_ != a_errno ) {
                const char* const str = strerror(a_errno);
                if ( nullptr != str ) {
                    str_ = std::to_string(a_errno) + " - " + std::string(str);
                } else {
                    str_ = std::to_string(a_errno) + " - ????";
                }
            } else {
                str_ = "";
            }
            no_ = a_errno;
        }
        
        inline void operator = (const std::string& a_message)
        {
            msg_ = a_message;
        }
        
        inline void operator = (const std::pair<const char* const, int>& a_fl)
        {
            fnc_ = a_fl.first;
            ln_  = a_fl.second;
        }
        
    public: // Access Method(s) / Function(s)
        
        inline const errno_t& no () const
        {
            return no_;
        }
        
        inline const std::string& str () const
        {
            return str_;
        }
        
        inline const std::string& message () const
        {
            return msg_;
        }
        
        inline const char* function () const
        {
            return fnc_.c_str();
        }
        
        inline const int& line () const
        {
            return ln_;
        }
        
    }; // end of class 'Error'

} // end of namespace 'sys'

#endif // NRS_SYS_ERROR_H_
