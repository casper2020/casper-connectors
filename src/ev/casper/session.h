/**
 * @file session.h
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
#ifndef NRS_EV_CASPER_SESSION_H
#define NRS_EV_CASPER_SESSION_H

#include "ev/redis/session.h"

#include <string>     // std::string
#include <set>        // std::set
#include <map>        // std::map
#include <algorithm> // std::transform

#include "json/json.h"

namespace ev
{
    
    namespace casper
    {
        
        class Session : public redis::Session
        {

        private: // Data
            
            std::string           json_api_url_;
            std::set<std::string> fields_;
            
        public: // Constructor(s) / Destructor
            
            Session (const Loggable::Data& a_loggable_data,
                     const std::string& a_iss, const std::string& a_sid, const std::string& a_token_prefix,
                     const bool a_test_maintenance_flag);
            Session (const Session& a_session);
            virtual ~Session ();
            
        protected: // Inherited Virtual Method(s) / Function(s)
            
            virtual std::string GetMaintenanceKey ();
            
        public: // Inline Method(s) / Function(s)
            
            void                            SetJSON_API_URL (const std::string& a_url);
            const std::string&              JSON_API_URL    () const;
            
            const std::string               GetValue        (const std::string& a_key, const std::string& a_if_empty) const;
            const char*                     GetValueCstr    (const char* a_key, const char* a_if_empty) const;
            
            void                            SetFields       (const Json::Value a_array);
            const std::set<std::string>&    GetFields       () const;
            
            void                            GetHeaders (const std::set<std::string>& a_fields,
                                                        std::map<std::string, std::string>& a_map) const;
        public: // Method(s) / Function(s)
            
            void               Patch           (Json::Value& a_object, const std::string& a_origin_ip_addr) const;
            void               Patch           (std::string& a_string, const std::string& a_origin_ip_addr) const;

        protected: // Method(s) / Function(s)
            
            void               Patch           (const std::string& a_name, Json::Value& a_object, const std::map<std::string, std::string>& a_patchables) const;
            
        };
        
        /**
         * @brief Search for a \link std::string \link value for a key.
         *
         * @param a_key
         * @param a_if_empty
         *
         * @return The \link std::string \link for the provided key or \link a_if_empty \link if not found.
         */
        inline const std::string Session::GetValue (const std::string& a_key, const std::string& a_if_empty) const
        {
            const auto it = data_.payload_.find(a_key);
            if ( data_.payload_.end() == it ) {
                return a_if_empty;
            }
            return it->second;
        }
        
        /**
         * @brief Search for a \link std::string \link value for a key.
         *
         * @param a_key
         * @param a_if_empty
         *
         * @return The \link std::string \link for the provided key or \link a_if_empty \link if not found.
         */
        inline const char* Session::GetValueCstr (const char* a_key, const char* a_if_empty) const
        {
            const auto it = data_.payload_.find(a_key);
            if ( data_.payload_.end() == it ) {
                return a_if_empty;
            }
            return it->second.c_str();
        }
        
        /**
         * @brief Set JSON API URL.
         *
         * @param a_url
         */
        inline void Session::SetJSON_API_URL (const std::string& a_url)
        {
            json_api_url_ = a_url;
        }
        
        /**
         * @return The JSON API URL.
         */
        inline const std::string& Session::JSON_API_URL () const
        {
            return json_api_url_;
        }
        
        /**
         * @brief Set interest fields.
         *
         * @param a_array JSON Array of strings.
         */
        inline void Session::SetFields (const Json::Value a_array)
        {
            fields_.clear();
            for ( Json::ArrayIndex idx = 0 ; idx < a_array.size() ; ++idx ) {
                fields_.insert(a_array[idx].asString());
            }
        }
        
        /**
         * @return Interest fields.
         *
         * @param a_array JSON Array of strings.
         */
        inline const std::set<std::string>& Session::GetFields () const
        {
            return fields_;
        }
        
        /**
         * @return Fields as X-CASPER-* headers.
         *
         * @param a_fields Set of interest fields.
         * @param a_map    Map of X-CASPER* headers -> values.
         */
        inline void Session::GetHeaders (const std::set<std::string>& a_fields, std::map<std::string, std::string>& a_map) const
        {
            const std::set<std::string>& set = ( a_fields.size() > 0 ? a_fields : fields_ );
            a_map.clear();
            for ( auto field : set ) {
                std::string name = "X-CASPER-" + field;
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);
                for ( std::string::size_type idx = 0 ; ( idx = name.find('_', idx) ) != std::string::npos ; ) {
                    name.replace(idx, 1, "-");
                    idx += 1;
                }
                a_map[name] = GetValue(field, "");
            }
        }
        
    } // end of namespace 'casper'
    
} // end of namespace 'ev'

#endif // NRS_EV_CASPER_SESSION_H
