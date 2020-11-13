/**
 * @file json.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-fs-assistant.
 *
 * casper-fs-assistant is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-fs-assistant  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-fs-assistant.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_EASY_JSON_H_
#define NRS_CC_EASY_JSON_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "json/json.h"

namespace cc
{

    namespace easy
    {

        template <class Exception> class JSON : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        private: // Helper(s)
            
            Json::FastWriter fw_;
            
        public: // Constructor(s) / Destructor
            
            JSON ();
            virtual ~JSON();
            
        public: // Method(s) / Function(s)
            
            const Json::Value& Get  (const Json::Value& a_parent, const char* const a_key, const Json::ValueType& a_type, const Json::Value* a_default,
                                     const char* const a_error_prefix_msg = "Invalid or missing ") const;

            const Json::Value& Get  (const Json::Value& a_parent, const std::vector<std::string>& a_keys, const Json::ValueType& a_type, const Json::Value* a_default,
                                     const char* const a_error_prefix_msg = "Invalid or missing ") const;

            const Json::Value& Get  (const Json::Value& a_array, const Json::ArrayIndex a_index, const Json::ValueType& a_type, const Json::Value* a_default,
                                     const char* const a_error_prefix_msg = "Invalid or missing ") const;

            void               Merge (Json::Value& a_lhs, const Json::Value& a_rhs) const;
                        
            void               Parse (const std::string& a_value, Json::Value& o_value) const;
            std::string        Write (const Json::Value& a_value);
            
        public: // Inline Method(s) / Function(s)
            
            const char* const ValueTypeAsCString (const Json::ValueType& a_type) const;
            
        }; // end of class 'JSON'
    
    
        /**
         * @brief Default constructor.
         */
        template <class E>
        cc::easy::JSON<E>::JSON ()
        {
            /* empty */
        }

        /**
         * @brief Destructor.
         */
        template <class E>
        cc::easy::JSON<E>::~JSON ()
        {
            /* empty */
        }

        /**
         * @brief Retrieve a JSON value from an array or defaults to the provided value ( if not null ).
         *
         * @param a_param
         * @param a_key
         * @param a_type
         * @param a_default
         *
         * @return
         */
        template <class E>
        const Json::Value& cc::easy::JSON<E>::Get (const Json::Value& a_parent, const char* const a_key, const Json::ValueType& a_type, const Json::Value* a_default,
                                                   const char* const a_error_prefix_msg) const
        {
            // ... try to obtain a valid JSON object ..
            try {
                const Json::Value& value = a_parent[a_key];
                if ( true == value.isNull() ) {
                    if ( nullptr != a_default ) {
                        return *a_default;
                    } else if ( Json::ValueType::nullValue == a_type ) {
                        return Json::Value::null;
                    } /* else { } */
                } else if ( value.type() == a_type || true == value.isConvertibleTo(a_type) ) {
                    return value;
                }
                // ... if it reached here, requested object is not set or has an invalid type ...
                throw E("%sJSON value for key '%s' - type mismatch: got %s, expected %s!",
                        a_error_prefix_msg,
                        a_key, ValueTypeAsCString(value.type()), ValueTypeAsCString(a_type)
                );
            } catch (const Json::Exception& a_json_exception ) {
                throw E("%s", a_json_exception.what());
            }
        }
    
        /**
         * @brief Retrieve a JSON value from an array or defaults to the provided value ( if not null ).
         *
         * @param a_param
         * @param a_keys
         * @param a_type
         * @param a_default
         *
         * @return
         */
        template <class E>
        const Json::Value& cc::easy::JSON<E>::Get (const Json::Value& a_parent, const std::vector<std::string>& a_keys, const Json::ValueType& a_type, const Json::Value* a_default,
                                                   const char* const a_error_prefix_msg) const
        {
            // ... try to obtain a valid JSON object ..
            std::string key;
            for ( auto k : a_keys ) {
                if ( true == a_parent.isMember(k) ) {
                    key = k;
                    break;
                }
            }
            // ... found? ...
            if ( 0 != key.length()  ) {
                return Get(a_parent, key.c_str(), a_type, a_default, a_error_prefix_msg);
            }
            // ... NOT found ...
            for ( auto idx = 0 ; idx < a_keys.size() - 1; ++idx  ) {
                key += a_keys[idx] + "||";
            }
            if ( a_keys.size() > 0 ) {
                key += a_keys[a_keys.size() - 1];
            }
            throw E("%sJSON value for key '%s' - type mismatch: got %s, expected %s!",
                    a_error_prefix_msg,
                    key.c_str(), "null", ValueTypeAsCString(a_type)
            );
        }

        /**
         * @brief Retrieve a JSON value from an array or defaults to the provided value ( if not null ).
         *
         * @param a_array
         * @param a_index
         * @param a_type
         * @param a_default
         *
         * @return
         */
        template <class E>
        const Json::Value& cc::easy::JSON<E>::Get (const Json::Value& a_array, const Json::ArrayIndex a_index, const Json::ValueType& a_type, const Json::Value* a_default,
                                                   const char* const a_error_prefix_msg) const
        {
            // ... try to obtain a valid JSON object ..
            try {
                const Json::Value& value = a_array[a_index];
                if ( true == value.isNull() ) {
                    if ( nullptr != a_default ) {
                        return *a_default;
                    } else if ( Json::ValueType::nullValue == a_type ) {
                        return Json::Value::null;
                    } /* else { } */
                } else if ( value.type() == a_type || true == value.isConvertibleTo(a_type) ) {
                    return value;
                }
                // ... if it reached here, requested object is not set or has an invalid type ...
                throw E("%sJSON value for index %d - type mismatch: got %s, expected %s!",
                        a_error_prefix_msg,
                        int(a_index), ValueTypeAsCString(value.type()), ValueTypeAsCString(a_type)
                );
            } catch (const Json::Exception& a_json_exception ) {
                throw E("%s", a_json_exception.what());
            }
        }
    
        /**
         * @brief Serialize a JSON string to a JSON Object.
         *
         * @param a_value JSON string to parse.
         * @param o_value JSON object to fill.
         */
        template <class E>
        void cc::easy::JSON<E>::Parse (const std::string& a_value, Json::Value& o_value) const
        {
            try {
                Json::Reader reader;
                if ( false == reader.parse(a_value, o_value) ) {
                    const auto errors = reader.getStructuredErrors();
                    if ( errors.size() > 0 ) {
                        throw E("An error ocurred while parsing '%s as JSON': %s!",
                                a_value.c_str(), reader.getFormatedErrorMessages().c_str()
                        );
                    } else {
                        throw E("An error ocurred while parsing '%s' as JSON!",
                                a_value.c_str()
                        );
                    }
                }
            } catch (const Json::Exception& a_json_exception ) {
                throw E("%s", a_json_exception.what());
            }
        }
    
        /**
         * @brief Serialize a JSON object to a string.
         *
         * @param o_value JSON object to serialize.
         *
         * @return JSON object as string.
         */
        template <class E>
        std::string cc::easy::JSON<E>::Write (const Json::Value& a_value)
        {
            return fw_.write(a_value);
        }

        /**
         * @brief Merge JSON Value.
         *
         * @param a_lhs Primary value.
         * @param a_rhs Override value.
         */
        template <class E>
        void cc::easy::JSON<E>::Merge (Json::Value& a_lhs, const Json::Value& a_rhs) const
        {
            if ( false == a_lhs.isObject() || false == a_rhs.isObject() ) {
                return;
            }
            for ( const auto& k : a_rhs.getMemberNames() ) {
                if ( true == a_lhs[k].isObject() && true == a_rhs[k].isObject() ) {
                    Merge(a_lhs[k], a_rhs[k]);
                } else {
                    a_lhs[k] = a_rhs[k];
                }
            }
        }
    
        /**
         * @return \link Json::ValueType \link string representation.
         */
        template <class Exception>
        inline const char* const JSON<Exception>::ValueTypeAsCString (const Json::ValueType& a_type) const
        {
            
            switch (a_type) {
                case Json::ValueType::nullValue:
                    return "null";
                case Json::ValueType::intValue:
                    return "int";
                case Json::ValueType::uintValue:
                    return "uint";
                case Json::ValueType::realValue:
                    return "real";
                case Json::ValueType::stringValue:
                    return "string";
                case Json::ValueType::booleanValue:
                    return "boolean";
                case Json::ValueType::arrayValue:
                    return "array";
                case Json::ValueType::objectValue:
                    return "object";
                default:
                    return "???";
            }
        
        }
    
    } // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_JSON_H_
