/**
 * @file tracker.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-nginx-broker.
 *
 * casper-nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef NRS_CC_ERRORS_TRACKER_H_
#define NRS_CC_ERRORS_TRACKER_H_

#include "json/json.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

#include "unicode/unistr.h"
#include "unicode/utypes.h"
#include "unicode/msgfmt.h"

#pragma clang diagnostic pop

namespace cc
{
    
    namespace errors
    {
        
        class Tracker
        {
            
        public: // Const Data
            
            const std::string content_type_;
            const std::string locale_;
            const std::string generic_error_message_key_;
            const std::string generic_error_message_with_code_key_;
            
        protected: // Data
            
            Json::Value array_;
            
        public: // Constructor (s) / Destructor
            
            Tracker (const std::string& a_content_type, const std::string& a_locale,
                     const char* const a_generic_error_message_key, const char* const a_generic_error_message_with_code_key);
            virtual ~Tracker ();
            
        public: // Pure Virtual Method(s) / Function(s)
            
            /**
             * @brief Track an error code.
             *
             * @param a_error_code
             * @param a_http_status_code
             * @param a_i18n_key
             */
            virtual void Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                const char* const a_i18n_key) = 0;
            
            /**
             * @brief Track an error code with an internal error message.
             *
             * @param a_error_code
             * @param a_http_status_code
             * @param a_i18n_key
             * @param a_internal_error_message.
             */
            virtual void Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                const char* const a_i18n_key, const char* const a_internal_error_msg) = 0;
            
            /**
             * @brief Track an error code with 'replaceable' arguments.
             *
             * @param a_error_code
             * @param a_http_status_code
             * @param a_i18n_key
             * @param a_args
             * @param a_args_count
             */
            virtual void Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count) = 0;
            
            /**
             * @brief Track an error code with 'replaceable' arguments and an internal error message.
             *
             * @param a_error_code
             * @param a_http_status_code
             * @param a_i18n_key
             * @param a_args
             * @param a_args_count
             * @param a_internal_error_msg
             */
            virtual void Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count, const char* const a_internal_error_msg) = 0;
            
            /**
             * @brief Transform collected errors to a JSON object.
             *
             * @param o_object
             */
            virtual void jsonify (Json::Value& o_object) = 0;
            
            /**
             * @brief Serialize collected errors in to a JSON string.
             *
             * @return A string in JSON format with all collected errors.
             */
            virtual std::string Serialize2JSON () const = 0;
            
        public: // Method(s) / Function(s)
            
            const std::string& ContentType () const;
            size_t             Count       () const;
            void               Reset       ();
            
        }; // end of class 'Tracker'
        
        
        /**
         * @return The JSON content type.
         */
        inline const std::string& Tracker::ContentType () const
        {
            return content_type_;
        }
        
        /**
         * @return The number of errors.
         */
        inline size_t Tracker::Count () const
        {
            return static_cast<size_t>(array_.size());
        }
        
        /**
         * @brief Forget tracked errors.
         */
        inline void Tracker::Reset ()
        {
            array_ = array_ = Json::Value(Json::ValueType::arrayValue);
        }
        
        
    } // end of namespace 'errors'
    
} // end of namespace 'cc'

#endif // NRS_CC_ERRORS_TRACKER_H_
