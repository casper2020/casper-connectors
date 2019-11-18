/**
 * @file value.h - REDIS Value
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
#ifndef NRS_EV_REDIS_VALUE_H_
#define NRS_EV_REDIS_VALUE_H_

#include <stdint.h>
#include <inttypes.h>

#include <string>     // std::string
#include <vector>     // std::vector
#include <sstream>    // std::stringstream
#include <functional> // std::function

#include "ev/object.h"
#include "ev/exception.h"

#include "ev/redis/includes.h"

#include "json/json.h"

namespace ev
{
    namespace redis
    {
        
        class Value final : public ev::Object
        {
            
        public: // Enum(s)
            
            enum class ContentType : uint16_t
            {
                String  = 1, // REDIS_REPLY_STRING
                Array   = 2, // REDIS_REPLY_ARRAY
                Integer = 3, // REDIS_REPLY_INTEGER
                Nil     = 4, // REDIS_REPLY_NIL
                Status  = 5, // REDIS_REPLY_STATUS
                Error   = 6  // REDIS_REPLY_ERROR
            };
            
        public: // Static Data
            
            static Value k_nil_;
            
        protected: // Data
            
            ContentType         content_type_;  //!< One of \link ContentType \link.
            std::string         string_value_;  //!< String value.
            long long           integer_value_; //!< Integer value.
            std::vector<Value*> array_value_;   //!< Array of values.
            
        public: // Constructor(s) / Destructor
            
            Value ();
            Value (const Value& a_value);
            Value (const struct redisReply* a_reply);
            virtual ~Value ();

        public: // Overloaded Operator(s)
            
            void         operator=  (const std::string& a_value);
            void         operator=  (const long long a_value);
            void         operator=  (const Value& a_value);
            void         operator=  (const struct redisReply* a_reply);
                  Value& operator[] (int a_index);
            const Value& operator[] (int a_index) const;
            
        public: // Inline Method(s) / Function(s)
            
            const ContentType  content_type () const;
            
            const bool         IsNil        () const;
            
            const bool         IsString     () const;
            const std::string& String       () const;
            
            const bool         IsInteger    () const;
            const long long    Integer      () const;
            
            const bool         IsArray      () const;
            const size_t       Size         () const;
            
            const bool         IsError      () const;
            const std::string& Error        () const;

            const bool         IsStatus     () const;
            const std::string& Status       () const;

            void               IterateHash  (const std::function<void(const Value* a_key, const Value* a_value)>& a_callback) const;
            
            Json::Value        AsJSONObject () const;
            
        private: // Inline Method(s) / Function(s)
            
            void Reset (const ContentType& a_content_type);
            void Copy  (const Value& a_value);
            void Set   (const struct redisReply* a_reply);
            
        };
        
        /**
         * @brief Operator '=' overload.
         *
         * @param a_value String value.
         */
        inline void Value::operator= (const std::string& a_value)
        {
            Reset(ContentType::String);
            string_value_ = a_value;
        }
        
        /**
         * @brief Operator '=' overload.
         *
         * @param a_value Numeric value.
         */
        inline void Value::operator= (const long long a_value)
        {
            Reset(ContentType::Integer);
            integer_value_ = a_value;
        }
        
        /**
         * @brief Operator '=' overload.
         *
         * @param a_value \link Value \link to copy.
         */
        inline void Value::operator= (const Value& a_value)
        {
            Reset(a_value.content_type_);
            switch (content_type_) {
                    
                case ev::redis::Value::ContentType::String:
                case ev::redis::Value::ContentType::Status:
                case ev::redis::Value::ContentType::Error:
                {
                    string_value_ = a_value.string_value_;
                    break;
                }
                    
                case ev::redis::Value::ContentType::Array:
                {
                    for ( auto object : a_value.array_value_ ) {
                        array_value_.push_back(new ev::redis::Value(*object));
                    }
                    break;
                }
                    
                case ev::redis::Value::ContentType::Integer:
                {
                    integer_value_ = a_value.integer_value_;
                    break;
                }
                    
                case ev::redis::Value::ContentType::Nil:
                default:
                    break;
                    
            }
        }
        
        /**
         * @brief Operator '=' overload.
         *
         * @param a_value \link struct redisReply \link to 'translate'.
         */
        inline void Value::operator=(const struct redisReply* a_reply)
        {
            Reset(Value::ContentType::Nil);
            if ( nullptr != a_reply ) {
                Set(a_reply);
            }
        }
        
        /**
         * @brief Operator '[]' overload.
         *
         * @param a_index
         */
        inline Value& Value::operator[] (int a_index)
        {
            if ( ContentType::Array != content_type_ ) {
                throw ev::Exception("Data object is not an array!");
            }
            if ( a_index < 0 || static_cast<size_t>(a_index) >= array_value_.size() ) {
                throw ev::Exception("Index out of bounds!");
            }
            return *array_value_[static_cast<size_t>(a_index)];
        }
        
        /**
         * @brief Operator '[]' overload.
         *
         * @param a_index
         */
        inline const Value& Value::operator[] (int a_index) const
        {
            if ( ContentType::Array != content_type_ ) {
                throw ev::Exception("Data object is not an array!");
            }
            if ( a_index < 0 || static_cast<size_t>(a_index) >= array_value_.size() ) {
                throw ev::Exception("Index out of bounds!");
            }
            return *array_value_[static_cast<size_t>(a_index)];
        }

        /**
         * @return The value content type, one of \link Value::ContentType \link.
         */
        inline const Value::ContentType Value::content_type () const
        {
            return content_type_;
        }

        /**
         * @return True when the object is 'nil'.
         */
        inline const bool Value::IsNil () const
        {
            return ( Value::ContentType::Nil == content_type_ );
        }
        
        /**
         * @return True when the object is a 'string'.
         */
        inline const bool Value::IsString () const
        {
            return ( Value::ContentType::String == content_type_ );
        }
        
        /**
         * @return String value.
         */
        inline const std::string& Value::String () const
        {
            return string_value_;
        }
        
        /**
         * @return True when the object is an 'integer'.
         */
        inline const bool Value::IsInteger () const
        {
            return ( Value::ContentType::Integer == content_type_ );
        }
        
        /**
         * @return Integer value.
         */
        inline const long long Value::Integer () const
        {
            return integer_value_;
        }
        
        /**
         * @return True when the object is an 'array'.
         */
        inline const bool Value::IsArray () const
        {
            return ( ContentType::Array == content_type_ );
        }
        
        /**
         * @return The 'array' object size.
         */
        inline const size_t Value::Size () const
        {
            if ( ContentType::Array != content_type_ ) {
                throw ev::Exception("Data object is not an array!");
            }
            return array_value_.size();
        }
        
        /**
         * @return True when the object is an 'error'.
         */
        inline const bool Value::IsError () const
        {
            return ( ContentType::Error == content_type_ );
        }
        
        /**
         * @return The 'error' message.
         */
        inline const std::string& Value::Error () const
        {
            return string_value_;
        }
        
        /**
         * @return True when the object is an 'status'.
         */
        inline const bool Value::IsStatus () const
        {
            return ( ContentType::Status == content_type_ );
        }
        
        /**
         * @return The 'status' message.
         */
        inline const std::string& Value::Status () const
        {
            return string_value_;
        }
        
        /**
         * @brief This method can be called to iterate an array as an hash.
         *
         * @param a_callback
         */
        inline void Value::IterateHash (const std::function<void(const Value* a_key, const Value* a_value)>& a_callback) const
        {
            if ( ContentType::Array != content_type_ ) {
                throw ev::Exception("Data object cannot be iterated as an hash - content is not an array!");
            }
            if ( 0 != ( array_value_.size() % 2 ) ) {
                throw ev::Exception("Data object cannot be iterated as an hash - not enough pairs!");
            }
            for ( size_t idx = 0; idx < array_value_.size() ; idx += 2 ) {
                a_callback(array_value_[idx], array_value_[idx+1]);
            }
        }
        
        /**
         * @return A JSON object representation of this REDIS object.
         */
        inline Json::Value Value::AsJSONObject () const
        {
            Json::Value value = Json::Value::null;
            
            switch(content_type_) {
                case ContentType::String:
                {
                    Json::Reader reader;
                    if ( false == reader.parse(string_value_, value) ) {
                        const auto errors = reader.getStructuredErrors();
                        if ( errors.size() > 0 ) {
                            throw ::ev::Exception( "An error occurred while parsing JSON subscription message: %s!" ,
                                                  errors[0].message.c_str()
                            );
                        } else {
                            throw ::ev::Exception("An error occurred while parsing JSON subscription message!");
                        }
                    }
                }
                    break;
                default:
                    throw ev::Exception("Unable to convert a REDIS object to a JSON object - not implemented for content type %d!",
                                        (int)content_type_
                );
            }
            
            return value;
        }
        
        /**
         * @brief Reset the current loaded data.
         *
         * @param a_content_type
         */
        inline void Value::Reset (const ContentType& a_content_type)
        {
            content_type_ = a_content_type;
            for ( auto value : array_value_ ) {
                delete value;
            }
            array_value_.clear();
        }
        
        /**
         * @brief Copy data from another object.
         *
         * @param a_value
         */
        inline void Value::Copy (const Value& a_value)
        {
            content_type_  = a_value.content_type_;
            integer_value_ = 0;
            
            switch (content_type_) {
                    
                case ev::redis::Value::ContentType::String:
                case ev::redis::Value::ContentType::Status:
                case ev::redis::Value::ContentType::Error:
                {
                    string_value_ = a_value.string_value_;
                    break;
                }
                    
                case ev::redis::Value::ContentType::Array:
                {
                    for ( auto object : a_value.array_value_ ) {
                        array_value_.push_back(new ev::redis::Value(*object));
                    }
                    break;
                }
                    
                case ev::redis::Value::ContentType::Integer:
                {
                    integer_value_ = a_value.integer_value_;
                    break;
                }
                    
                case ev::redis::Value::ContentType::Nil:
                default:
                    break;
                    
            }
        }
        
        /**
         * @brief Set current value from a \link redisReply \link.
         */
        inline void Value::Set (const struct redisReply* a_reply)
        {
            // ... create object from reply ...
            switch (a_reply->type) {
                    
                case REDIS_REPLY_STRING: // 1
                {
                    // The status string can be accessed using reply->str.
                    // The length of this string can be accessed using reply->len ...
                    content_type_ = Value::ContentType::String;
                    if ( nullptr != a_reply->str && a_reply->len > 0 ) {
                        string_value_ = std::string(a_reply->str, a_reply->len);
                    }
                    break;
                }
                    
                case REDIS_REPLY_ARRAY: // 2
                {
                    // A multi bulk reply.
                    // The number of elements in the multi bulk reply is stored in reply->elements.
                    // Every element in the multi bulk reply is a redisReply object as well and can be accessed via reply->element[..index..].
                    // Redis may reply with nested arrays but this is fully supported.
                    content_type_ = Value::ContentType::Array;
                    for ( size_t idx = 0 ; idx < a_reply->elements ; ++idx ) {
                        array_value_.push_back(new Value(a_reply->element[idx]));
                    }
                    break;
                }
                    
                case REDIS_REPLY_INTEGER: // 3
                {
                    // The integer value can be accessed using the reply->integer field of type long long.
                    content_type_  = Value::ContentType::Integer;
                    integer_value_ = a_reply->integer;
                    break;
                }
                    
                case REDIS_REPLY_NIL: // 4
                {
                    // The command replied with a nil object. There is no data to access.
                    content_type_ = Value::ContentType::Nil;
                    break;
                }
                    
                case REDIS_REPLY_STATUS: // 5
                {
                    // The status string can be accessed identical to REDIS_REPLY_STRING.
                    content_type_ = Value::ContentType::Status;
                    if ( nullptr != a_reply->str && a_reply->len > 0 ) {
                        string_value_ = std::string(a_reply->str, a_reply->len);
                    }
                    break;
                }
                    
                case REDIS_REPLY_ERROR: // 6
                {
                    // The error string can be accessed identical to REDIS_REPLY_STRING.
                    content_type_ = Value::ContentType::Error;
                    if ( nullptr != a_reply->str && a_reply->len > 0 ) {
                        string_value_ = std::string(a_reply->str, a_reply->len);
                    }
                    break;
                }
                    
                default:
                {
                    throw ev::Exception("Don't know how to handle redis reply type '%d'!", a_reply->type);
                }
            }
        }

    } // end of namespace 'redis'
    
} // end of namespace 'ev'

#endif // NRS_EV_REDIS_VALUE_H_
