/**
 * @file singleton.h
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
#ifndef NRS_CC_SINGLETON_H_
#define NRS_CC_SINGLETON_H_

#include "osal/osal_singleton.h"

#include <string>      // std::string
#include <functional>  // std::function

#include "json/json.h"

namespace cc
{
    
    namespace i18n
    {
        
        // ---- //
        class Singleton;
        class SingletonOneShot final : public ::osal::Initializer<Singleton>
        {
            
        public: // Constructor(s) / Destructor
            
            SingletonOneShot (Singleton& a_instance)
                : ::osal::Initializer<Singleton>(a_instance)
            {
                /* empty */
            }
            virtual ~SingletonOneShot ()
            {
                /* empty */
            }
            
        }; // end of class 'SingletonOneShot'
        
        class Singleton final : public osal::Singleton<Singleton, SingletonOneShot>
        {
            
        public: // Const Static Data
            
            static const std::map<uint16_t, std::string> k_http_status_codes_map_;
            
        private: // Data
            
            Json::Value localization_;
            
        public: // One-shot Call Method(s) / Function(s)
            
            void Startup  (const std::string& a_resources_dir,
                           const std::function<void(const char* const a_i18_key, const std::string& a_file, const std::string& a_reason)>& a_failure_callback);
            void Shutdown ();
            
        public: // Method(s) / Function(s)
            
            const Json::Value Get           (const std::string& a_locale, const char* const a_key) const;
            bool              IsInitialized () const;
            bool              Contains      (const std::string& a_locale) const;
            
        }; // end of class 'Singleton'
        
        /**
         * @brief Search for a \link Json::Value \link for a specific locale-key combination.
         *
         * @param a_locale
         * @param a_key
         *
         * @return A a \link Json::Value \link for the provide locale-key combination.
         */
        inline const Json::Value Singleton::Get (const std::string& a_locale, const char* const a_key) const
        {
            if ( true == localization_.isNull() || false == localization_.isObject() ) {
                return Json::Value::null;
            }
            
            Json::Value dict_for_locale = localization_.get(a_locale, Json::Value::null);
            if ( true == dict_for_locale.isNull() ) {
                dict_for_locale = localization_.get("en_US", Json::Value::null);
            }
            
            if ( true == dict_for_locale.isNull() ) {
                return Json::Value::null;
            }
            
            return dict_for_locale.get(a_key, Json::Value::null);
        }
        
        /**
         * @return True if it is initialized, false otherwise.
         */
        inline bool Singleton::IsInitialized () const
        {
            return ( false == localization_.isNull() );
        }
        
        /**
         * @return True if a locale is loaded, false otherwise.
         */
        inline bool Singleton::Contains (const std::string& a_locale) const
        {
            return ( true == IsInitialized() && false == localization_.get(a_locale, Json::Value::null).isNull() );
        }
        
    } // end of namespace 'i18n'
    
} // end of namespace 'cc'

#endif // NRS_CC_SINGLETON_H_
