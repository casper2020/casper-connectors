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
#include <functional> // std::bad_function_call

#include "cc/codes.h"

#if defined(__APPLE__) || defined(__clang__)
    #define STD_CPP_GENERIC_EXCEPTION_TRACE() \
        [&] () -> std::string { \
            std::stringstream _tmp_ss; \
            std::exception_ptr p = std::current_exception(); \
            _tmp_ss << "C++ Generic Exception @" << __PRETTY_FUNCTION__ << ":" << __LINE__; \
            try { \
                std::rethrow_exception(p);\
            } catch(const std::exception& e) { \
                _tmp_ss << "what() =" << e.what(); \
            } \
            return _tmp_ss.str(); \
        }()
#else
    #define STD_CPP_GENERIC_EXCEPTION_TRACE() \
        [&] () -> std::string { \
            std::stringstream _tmp_ss; \
            std::exception_ptr p = std::current_exception(); \
            _tmp_ss << "C++ Generic Exception @" << __PRETTY_FUNCTION__ << ":" << __LINE__; \
            if ( p ) { \
                _tmp_ss << "name() =" << p.__cxa_exception_type()->name(); \
                _tmp_ss << "what() =" << p.__cxa_exception_type()->name(); \
            } \
            return _tmp_ss.str(); \
        }()
#endif

#define CC_EXCEPTION_RETHROW(a_unhandled) \
    ::cc::Exception::Rethrow(a_unhandled, __FILE__, __LINE__, __FUNCTION__)

namespace cc
{

    class Exception : public std::exception
    {
        
    protected: // Data
        
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
        
        /**
         * @brief Destructor.
         */
        virtual ~Exception ()
        {
            /* empty */
        }
        
    public: // overrides
        
        /**
         * @return An explanatory string.
         */
        virtual const char* what() const throw()
        {
            return what_.c_str();
        }
        
    public: // Static Method(s) / Function(s)
        
        /*
         * @brief Rethrow a standard exception.
         */
        static void Rethrow (const bool a_unhandled,
                             const char* const a_file, const int a_line, const char* const a_function)
        {
            const std::string msg_prefix = a_unhandled ? "An unhandled " : "An ";
            const std::string msg_suffix = " occurred at " + std::string(a_file)
                                           + ':' + std::to_string(a_line)
                                           + ", function " + std::string(a_function)
            ;
            if ( true == a_unhandled ) {
                try {
                    const auto e = std::current_exception();
                    if ( e ) {
                        std::rethrow_exception(std::current_exception());
                    } else {
                        throw ::cc::Exception(msg_prefix + "???" + msg_suffix + '.');
                    }
                } catch (const std::logic_error& a_rte) {
                    throw ::cc::Exception(msg_prefix + "logic error" + msg_suffix + ": " + a_rte.what());
                }  catch (const std::runtime_error& a_rte) {
                    throw ::cc::Exception(msg_prefix + "runtime error" + msg_suffix + ": " + a_rte.what());
                } catch (const std::bad_function_call& a_bfc) {
                    throw ::cc::Exception(msg_prefix + "bad function call" + msg_suffix + ": " + a_bfc.what());
                } catch (const std::exception& a_se) {
                    throw ::cc::Exception(msg_prefix + "exception" + msg_suffix + ": " + a_se.what());
                } catch (...) {
                    throw ::cc::Exception(msg_prefix + "???" + msg_suffix + '.');
                }
            } else {
                const auto e = std::current_exception();
                if ( e ) {
                    try {
                        std::rethrow_exception(e);
                    } catch(const std::exception& e) {
                        throw ::cc::Exception("%s", e.what());
                    }
                } else {
                    throw ::cc::Exception(msg_prefix + "???" + msg_suffix + '.');
                }
            }
        }
        
    protected: // Method(s) / Function(s)
        
        /**
         * @brief Set the exception message.
         *
         * @param a_format printf like format followed by a variable number of arguments.
         * @param a_list   Variable arguments list.
         */
        void SetMessage (const char* const a_format, va_list a_list)
        {
            std::va_list args;
            va_copy(args, a_list);
            auto temp   = std::vector<char> {};
            auto length = std::size_t { 512 };
            while ( temp.size() <= length ) {
                temp.resize(length + 1);
                va_copy(args, a_list);
                const auto status = std::vsnprintf(temp.data(), temp.size(), a_format, args);
                va_end(args);
                if ( status < 0 ) {
                    throw std::runtime_error {"string formatting error"};
                }
                length = static_cast<std::size_t>(status);
            }
            what_ = length > 0 ? std::string { temp.data(), length } : "";
            va_end(args);
        }
        
    }; // end of class 'Exception'

    class CodedException : public cc::Exception
    {
        
    public: // Const Data
        
        const uint16_t code_;
        
    public: // constructor / destructor
        
        CodedException (const std::string& a_message)                                          = delete;
        CodedException (const char* const a_format, ...) __attribute__((format(printf, 2, 3))) = delete;
        
        /**
         * @brief A constructor that provides the reason of the fault origin.
         *
         * @param a_code   uint16_t error code.
         * @param a_format printf like format followed by a variable number of arguments.
         * param ...
         */
        CodedException (const uint16_t a_code, const char* const a_format, ...) __attribute__((format(printf, 3, 4)))
         : Exception(""),
           code_(a_code)
        {
            std::va_list args;
            va_start(args, a_format);
            SetMessage(a_format, args);
            va_end(args);
        }
        
        /**
         * @brief Destructor.
         */
        virtual ~CodedException ()
        {
            /* empty */
        }
        
    }; // end of class 'CodedException'

#define CC_EXCEPTION_DECLARE(a_name, a_code) \
    class a_name final : public cc::CodedException \
    { \
    public: \
        a_name (const std::string& a_message) \
            : ::cc::CodedException(a_code, "%s", a_message.c_str()) \
        { \
            what_ = a_message; \
        } \
        a_name (const char* const a_format, ...) __attribute__((format(printf, 2, 3))) \
        : ::cc::CodedException(a_code, "%s", "") \
        { \
            std::va_list args; \
            va_start(args, a_format); \
            SetMessage(a_format, args); \
            va_end(args); \
        } \
        virtual ~a_name () { } \
    }

// 4xx client errors
CC_EXCEPTION_DECLARE(BadRequest         , CC_STATUS_CODE_BAD_REQUEST);
CC_EXCEPTION_DECLARE(NotFound           , CC_STATUS_CODE_NOT_FOUND);
// 5xx server errors
CC_EXCEPTION_DECLARE(InternalServerError, CC_STATUS_CODE_INTERNAL_SERVER_ERROR);
CC_EXCEPTION_DECLARE(NotImplemented     , CC_STATUS_CODE_NOT_IMPLEMENTED);
CC_EXCEPTION_DECLARE(GatewayTimeout     , CC_STATUS_CODE_GATEWAY_TIMEOUT);
}

#endif // NRS_CC_EXCEPTION_H_
