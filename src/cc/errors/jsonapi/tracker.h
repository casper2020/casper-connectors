/**
 * @file tracker.h
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
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef NRS_CC_ERRORS_JSONAPI_TRACKER_H_
#define NRS_CC_ERRORS_JSONAPI_TRACKER_H_

#include "cc/errors/tracker.h"

namespace cc
{
    
    namespace errors
    {
        
        namespace jsonapi
        {

            class Tracker : public cc::errors::Tracker
            {
                
            public: // Constructor (s) / Destructor
                
                Tracker (const std::string& a_locale, const std::string& a_content_type,
                         const char* const a_generic_error_message_key, const char* const a_generic_error_message_with_code_key);
                virtual ~Tracker ();
                
            public: // Inherited Method(s) / Function(s)
                
                virtual void        Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                           const char* const a_i18n_key);
                virtual void        Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                           const char* const a_i18n_key, const char* const a_internal_error_msg);
                virtual void        Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                           const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count);
                virtual void        Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                           const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count, const char* const a_internal_error_msg);
                virtual void        jsonify (Json::Value& o_object);
                
                virtual std::string Serialize2JSON () const;
                
            private: // Method(s) / Function(s)
                
                void Track (const char* const a_error_code, const uint16_t a_http_status_code,
                            const std::string& a_i18n_key, const std::string& a_detail,
                            const char* const a_internal);
                
            }; // end of class 'Tracker'

        } // end of namespace 'jsonapi'
        
    } // end of namespace 'errors'
    
} // end of namespace 'cc'

#endif // NRS_CC_ERRORS_JSONAPI_TRACKER_H_
