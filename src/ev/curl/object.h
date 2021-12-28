/**
 * @file object.h
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
#ifndef NRS_EV_CURL_OBJECT_H_
#define NRS_EV_CURL_OBJECT_H_

#include "ev/object.h"

#include <string>
#include <map>
#include <vector>

#include <strings.h> // strcasecmp

namespace ev
{

    namespace curl
    {

        class Object : public ev::Object
        {

        public: // Data Type(s)
            
            typedef struct _HeaderMapKeyComparator {
                
                const std::string& value_;
                
                _HeaderMapKeyComparator (const std::string& a_value)
                    : value_(a_value)
                {
                    /* empty */
                }
                
                bool operator() (const std::pair<std::string, std::string>& a_value) const
                {
                    return ( 0 == strcasecmp(value_.c_str(), a_value.first.c_str()) );
                }
                
            } HeaderMapKeyComparator;
            
            typedef struct _cURLHeaderMapKeyComparator {
                
                const std::string value_;
                
                _cURLHeaderMapKeyComparator (const char* const a_value)
                    : value_(a_value)
                {
                    /* empty */
                }

                _cURLHeaderMapKeyComparator (const std::string& a_value)
                    : value_(a_value)
                {
                    /* empty */
                }

                bool operator() (const std::pair<std::string, std::vector<std::string>>& a_value) const
                {
                    return ( 0 == strcasecmp(value_.c_str(), a_value.first.c_str()) );
                }
                
            } cURLHeaderMapKeyComparator;

            #define EV_CURL_HEADERS_MAP std::map<std::string, std::vector<std::string>>
            
        public: // Constructor(s) / Destructor

            Object (const Type& a_type);
            virtual ~Object ();

        }; // end of class 'Object'

    } // end of namespace 'curl'

} // end of namespace 'ev'

#endif // NRS_EV_CURL_OBJECT_H_
