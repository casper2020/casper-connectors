/**
 * @file value.h
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
#ifndef NRS_EV_CURL_VALUE_H_
#define NRS_EV_CURL_VALUE_H_

#include <sys/types.h>
#include <stdint.h> // uint16_t
#include <vector>
#include <set>
#include <string>
#include <algorithm> // std::find_if

#include <string.h>   // strlen

#include "ev/curl/object.h"
#include "ev/exception.h"

namespace ev
{
    namespace curl
    {

        class Value final : public curl::Object
        {

        private: // Const Data
            
            const size_t rtt_;

        private: // Data

           int                  code_;          //!< > 0 HTTP code, < 0 CURL ERROR CODE.
           EV_CURL_HEADERS_MAP  headers_;       //!< Collected headers.
           std::string          body_;          //!< Collected text data, nullptr if none.
           int64_t              last_modified_; //!< Last modified date.
            
        public: // Constructor(s) / Destructor

            Value   (const int a_code, const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body,
                     const size_t& a_rtt);
            Value   (const Value& a_value);
            virtual ~Value ();

        public: // Method(s) / Function(s)
            
            int                        code              () const;
            const EV_CURL_HEADERS_MAP& headers           () const;
            const std::string&         body              () const;
            const size_t               rtt               () const;
            void                       set_last_modified (int64_t a_timestamp);
            int64_t                    last_modified     () const;
            std::string                header            (const char* const a_name) const;
            std::string                header_value      (const char* const a_name) const;
            size_t                     header_values     (const char* const a_name, std::set<std::string>& o_values) const;
            void                       headers_as_map    (std::map<std::string, std::string>& o_map) const;

        public:
            
            inline Value& operator=(const Value&) = delete;
            
        public: // Static Method(s) / Function(s)
            
            bool content_disposition (std::string* o_disposition, std::string* o_field_name, std::string* o_file_name) const;
            
        };

        /**
         * @return R/O access to received code.
         */
        inline int Value::code () const
        {
            return code_;
        }

        /**
         * @return R/O access to received headers.
         */
        inline const EV_CURL_HEADERS_MAP& Value::headers () const
        {
            return headers_;
        }
    
        /**
         * @return R/O access to received data.
         */
        inline const std::string& Value::body () const
        {
            return body_;
        }
    
        /**
         * @return R/O access to RTT.
         */
        inline const size_t Value::rtt () const
        {
            return rtt_;
        }

        /**
         * @brief Set the last modified timestamp.
         *
         * @param a_timestamp Timestamp value.
         */
        inline void Value::set_last_modified (int64_t a_timestamp)
        {
            last_modified_ = a_timestamp;
        }

        /**
         * @return The last modified timestamp.
         */
        inline int64_t Value::last_modified () const
        {
            return last_modified_;
        }

        /**
         * @brief Rebuild header value.
         *
         * @param a_name Header name.
         *
         * @return Rebuilt value.
         */
        inline std::string Value::header (const char* const a_name) const
        {
            const auto p = std::find_if(headers_.begin(), headers_.end(), ev::curl::Object::cURLHeaderMapKeyComparator(a_name));
            if ( headers_.end() != p ) {
                return ( p->first + ":" + ( p->second.front().length() > 0 ? " " + p->second.front() : "" ) );
            } else {
                return "";
            }
        }
    
        /**
         * @brief Fetch header value.
         *
         * @param a_name Header name.
         *
         * @return Header value.
         */
        inline std::string Value::header_value (const char* const a_name) const
        {
            const auto p = std::find_if(headers_.begin(), headers_.end(), ev::curl::Object::cURLHeaderMapKeyComparator(a_name));
            if ( headers_.end() != p ) {
                return p->second.front();
            } else {
                return "";
            }
        }
    
        /**
         * @brief Fetch header values.
         *
         * @param a_name   Header name.
         * @param o_values Set of values for provided header name.
         *
         * @return Number of possible values for this header.
         */
        inline size_t Value::header_values (const char* const a_name, std::set<std::string>& o_values) const
        {
            o_values.clear();
            const auto p = std::find_if(headers_.begin(), headers_.end(), ev::curl::Object::cURLHeaderMapKeyComparator(a_name));
            if ( headers_.end() != p ) {
                for ( auto it : p->second ) {
                    o_values.insert(it);
                }
            }
            return o_values.size();
        }
        /**
         * @brief Rebuild headers map.
         *
         * @param o_map Rebuilt value.
         */
        inline void Value::headers_as_map (std::map<std::string, std::string>& o_map) const
        {
            for ( auto p : headers_ ) {
                o_map[p.first] = ( p.second.front().length() > 0 ? " " + p.second.front() : "" );
            }
        }

    } // end of namespace 'curl'

} // end of namespace 'ev'

#endif // NRS_EV_CURL_VALUE_H_
