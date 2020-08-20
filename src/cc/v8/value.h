/**
 * @file value.h
 *
 * Copyright (c) 2010-2018 Neto Ranito & Seabra LDA. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CASPER_CONNECTORS_V8_VALUE_H_
#define NRS_CASPER_CONNECTORS_V8_VALUE_H_

#include <sstream> // std::stringstream
#include <limits>  // std::numeric_limits
#include <regex>   // std::regex
#include <cmath>   // NAN

#include "json/json.h"

namespace cc
{
    
    namespace v8
    {
        
        class Value // TODO 2.0 CODE REVIEW
        {
            
        public: // Data Type(s)
            
            typedef std::map<std::string, cc::v8::Value> Map;
            
        private: // Data Type(s)
            
            // C++17 std::variant
            typedef union {
                int32_t  int32_;
                uint32_t uint32_;
                double   double_;
                bool     bool_;
            } ValueU;
            
        public:
            
            enum TermType : unsigned {
                Undefined  = 0x00,
                Number     = 0x01,
                Text       = 0x02,
                Date       = 0x04,
                Boolean    = 0x08,
                ExcelDate  = 0x10,
            };
            
            enum class Type : unsigned {
                Undefined,
                Int32,
                UInt32,
                Double,
                String,
                Boolean,
                Object,
                Null
            };
            
        public: // Const Data
            
            const TermType term_type_;
            
        private: // Const Data
            
            const std::string dummy_string_;
            
        private: // Data
            
            Type         type_;
            ValueU       value_;
            std::string  string_;
            bool         set_;
            Json::Value  object_;
            
        public: // Constructor(s) / Destructor
            
            Value ()
                : term_type_(TermType::Undefined), type_(Type::Undefined)
            {
                set_ = false;
            }
            
            Value (const Value& a_value)
                : term_type_(a_value.term_type_), type_(a_value.type_)
            {
                set_  = a_value.set_;
                switch (type_) {
                    case Type::Int32:
                        value_.int32_ = a_value.value_.int32_;
                        break;
                    case Type::UInt32:
                        value_.uint32_ = a_value.value_.uint32_;
                        break;
                    case Type::Double:
                        value_.double_ = a_value.value_.double_;
                        break;
                    case Type::Boolean:
                        value_.bool_ = a_value.value_.bool_;
                        break;
                    case Type::String:
                        string_ = a_value.string_;
                        break;
                    case Type::Object:
                        object_ = a_value.object_;
                    default:
                        set_  = false;
                        type_ = Type::Undefined;
                        break;
                }
            }
            
            Value (const TermType a_type)
                : term_type_(a_type), type_(Type::Undefined)
            {
                set_ = false;
            }
            
            Value (const int32_t a_value)
            : term_type_(TermType::Number), type_(Type::Int32)
            {
                set_          = true;
                value_.int32_ = a_value;
            }
            
            Value (const uint32_t a_value)
            : term_type_(TermType::Number), type_(Type::UInt32)
            {
                set_           = true;
                value_.uint32_ = a_value;
            }
            
            Value (const double a_value)
            : term_type_(TermType::Number), type_(Type::Double)
            {
                set_           = true;
                value_.double_ = a_value;
            }
            
            Value (const bool a_value)
            : term_type_(TermType::Boolean), type_(Type::Boolean)
            {
                set_         = true;
                value_.bool_ = a_value;
            }
            
            Value (const char* const a_value)
            : term_type_(TermType::Text), type_(Type::String)
            {
                set_    = true;
                string_ = a_value;
            }
            
            Value (const std::string& a_value)
            : term_type_(TermType::Text), type_(Type::String)
            {
                set_    = true;
                string_ = a_value;
            }
            
            Value (const Json::Value& a_value)
            : term_type_(TermType::Undefined), type_(Type::Object)
            {
                set_    = true;
                object_ = a_value;
            }
            
            virtual ~Value ()
            {
                /* empty */
            }
            
        public: // Method(s) / Function(s)
            
            inline Type type () const
            {
                return type_;
            }
            
            inline bool IsSet () const
            {
                return ( true == set_ && Type::Undefined != type_ );
            }
            
            inline void SetNull ()
            {
                set_  = true;
                type_ = Type::Null;
            }
            
            inline bool IsNull () const
            {
                return ( true == set_ && Type::Null == type_ );
            }
            
            inline bool IsUndefined () const
            {
                return ( false == set_ || Type::Undefined == type_ );
            }
            
            inline bool IsNumber () const
            {
                return ( true == IsSet() &&
                        ( Type::Int32 == type_  || Type::UInt32 == type_ || Type::Double == type_ )
                        );
            }
            
            inline bool IsObject () const
            {
                return ( true == IsSet() && Type::Object == type_ );
            }
            
            inline void operator = (const Value& a_value)
            {
                set_  = a_value.set_;
                type_ = a_value.type_;
                switch (type_) {
                    case Type::Int32:
                        value_.int32_ = a_value.value_.int32_;
                        break;
                    case Type::UInt32:
                        value_.uint32_ = a_value.value_.uint32_;
                        break;
                    case Type::Double:
                        value_.double_ = a_value.value_.double_;
                        break;
                    case Type::Boolean:
                        value_.bool_ = a_value.value_.bool_;
                        break;
                    case Type::String:
                        string_ = a_value.string_;
                        break;
                    case Type::Object:
                        object_ = a_value.object_;
                        break;
                    default:
                        set_  = false;
                        type_ = Type::Undefined;
                        break;
                }
            }
            
            inline void operator = (const char* const a_value)
            {
                set_    = true;
                string_ = a_value;
                type_   = Type::String;
            }
            
            inline operator const char* const () const
            {
                if ( Type::String  == type_ ) {
                    return string_.c_str();
                }
                return "";
            }
            
            inline void operator = (const std::string& a_value)
            {
                set_    = true;
                string_ = a_value;
                type_   = Type::String;
            }
            
            inline operator const std::string& () const
            {
                if ( Type::String  == type_ ) {
                    return string_;
                }
                return dummy_string_;
            }
            
            inline void operator = (const int32_t a_value)
            {
                set_          = true;
                value_.int32_ = a_value;
                type_         = Type::Int32;
            }
            
            inline operator int32_t () const
            {
                switch (type_) {
                    case Type::Int32:
                        return value_.int32_;
                    case Type::UInt32:
                        return static_cast<int32_t>(value_.uint32_);
                    case Type::Double:
                        return static_cast<int32_t>(value_.double_);
                    case Type::String:
                        return static_cast<int32_t>(std::stoll(string_));
                    default:
                        // TODO 2.0 v8 throw?
                        return std::numeric_limits<int32_t>::max();
                }
            }
            
            inline void operator = (const uint32_t a_value)
            {
                set_           = true;
                value_.uint32_ = a_value;
                type_          = Type::UInt32;
            }
            
            inline operator uint32_t () const
            {
                switch (type_) {
                    case Type::Int32:
                        return static_cast<uint32_t>(value_.int32_);
                    case Type::UInt32:
                        return value_.uint32_;
                    case Type::Double:
                        return static_cast<uint32_t>(value_.double_);
                    case Type::String:
                        return static_cast<uint32_t>(std::stoull(string_));
                    default:
                        // TODO 2.0 v8 throw?
                        return std::numeric_limits<uint32_t>::max();
                }
            }
            
            inline void operator = (const double a_value)
            {
                set_           = true;
                value_.double_ = a_value;
                type_          = Type::Double;
            }
            
            inline operator double () const
            {
                switch (type_) {
                    case Type::Int32:
                        return static_cast<double>(value_.int32_);
                    case Type::UInt32:
                        return static_cast<double>(value_.uint32_);
                    case Type::Double:
                        return value_.double_;
                    case Type::String:
                        return std::stod(string_);
                    default:
                        // TODO 2.0 v8 throw?
                        return NAN;
                }
            }
            
            inline void operator = (const bool a_value)
            {
                set_         = true;
                value_.bool_ = a_value;
                type_        = Type::Boolean;
            }
            
            inline operator const bool () const
            {
                if ( Type::Boolean == type_ ) {
                    return value_.bool_;
                }
                // TODO 2.0 v8 throw?
                return false;
            }
            
            inline void operator = (const Json::Value& a_value)
            {
                set_    = true;
                object_ = a_value;
                type_   = Type::Object;
            }
            
            inline operator const Json::Value& () const
            {
                if ( Type::Object == type_ ) {
                    return object_;
                }
                // TODO 2.0 v8 throw?
                return Json::Value::null;
            }
            
            inline std::string AsString () const
            {
                switch (type_) {
                    case Type::Int32:
                        return std::to_string(value_.int32_);
                    case Type::UInt32:
                        return std::to_string(value_.uint32_);
                    case Type::Double:
                        return std::to_string(value_.double_);
                    case Type::String:
                        return string_;
                    case Type::Boolean:
                        return ( value_.bool_ ? "true" : "false" );
                    case Type::Null:
                        return "null";
                    default:
                        // TODO 2.0 v8 throw?
                        return "undefined";
                }
            }
            
            inline double AsDouble () const
            {
                switch (type_) {
                    case Type::Int32:
                        return static_cast<double>(value_.int32_);
                    case Type::UInt32:
                        return static_cast<double>(value_.uint32_);
                    case Type::Double:
                        return value_.double_;
                    case Type::String:
                        try {
                            const std::regex params_expr("^[+-]?((\\d+(\\.\\d*)?)|(\\.\\d+))$", std::regex_constants::ECMAScript | std::regex_constants::icase);
                            auto tmp_begin = std::sregex_iterator(string_.begin(), string_.end(), params_expr);
                            auto tmp_end   = std::sregex_iterator();
                            // ... expecting exactly one match ...
                            if ( 1 == std::distance(tmp_begin, tmp_end) ) {
                                return std::stod(string_);
                            }
                        } catch (...) {
                            
                        }
                        return NAN;
                    case Type::Boolean:
                    case Type::Null:
                    default:
                        return NAN;
                }
            }
            
        }; // end of class 'Value'
        
    } // end of namespace 'v8'
    
} // end of namespace cc

#endif // NRS_CASPER_CONNECTORS_V8_VALUE_H_


