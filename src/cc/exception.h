/**
 * @file exception.h
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
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_EXCEPTION_H_
#define NRS_CC_EXCEPTION_H_

#include <string>     // std::string
#include <cstdarg>    // va_start, va_end, std::va_list
#include <cstddef>    // std::size_t
#include <stdexcept>  // std::runtime_error
#include <vector>     // std::vector
#include <sstream>    // std::stringstream

#ifdef __APPLE__
    #define STD_CPP_GENERIC_EXCEPTION_TRACE() \
        [&] () -> std::string { \
            std::stringstream ss; \
            std::exception_ptr p = std::current_exception(); \
            ss << "C++ Generic Exception @" << __PRETTY_FUNCTION__ << ":" << __LINE__; \
            try { \
                std::rethrow_exception(p);\
            } catch(const std::exception& e) { \
                ss << "what() =" << e.what(); \
            } \
            return ss.str(); \
        }()
#else
    #define STD_CPP_GENERIC_EXCEPTION_TRACE() \
        [&] () -> std::string { \
            std::stringstream ss; \
            std::exception_ptr p = std::current_exception(); \
            ss << "C++ Generic Exception @" << __PRETTY_FUNCTION__ << ":" << __LINE__; \
            if ( p ) { \
                ss << "name() =" << p.__cxa_exception_type()->name(); \
                ss << "what() =" << p.__cxa_exception_type()->name(); \
            } \
            return ss.str(); \
        }()
#endif

namespace cc
{

    class Exception : public std::exception
    {
        
    private: // Data
        
        std::string what_;
        
    public: // constructor / destructor
        
        /**
         * @brief A constructor that provides the reason of the fault origin.
         *
         * @param a_message Reason of the fault origin.
         */
        Exception (const std::string& a_message)
        {
            what_ = a_message;
        }
        
        /**
         * @brief A constructor that provides the reason of the fault origin.
         *
         * @param a_format printf like format followed by a variable number of arguments.
         * param ...
         */
        Exception (const char* const a_format, ...) __attribute__((format(printf, 2, 3)))
        {
            auto temp   = std::vector<char> {};
            auto length = std::size_t { 512 };
            std::va_list args;
            while ( temp.size() <= length ) {
                temp.resize(length + 1);
                va_start(args, a_format);
                const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
                va_end(args);
                if ( status < 0 ) {
                    throw std::runtime_error {"string formatting error"};
                }
                length = static_cast<std::size_t>(status);
            }
            what_ = length > 0 ? std::string { temp.data(), length } : "";
        }
        
    public: // overrides
        
        /**
         * @return An explanatory string.
         */
        virtual const char* what() const throw()
        {
            return what_.c_str();
        }
        
    };

}

#endif // NRS_CC_EXCEPTION_H_
