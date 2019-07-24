/**
 * @file request.h - PostgreSQL
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
#ifndef NRS_EV_POSTGRESQL_REQUEST_H_
#define NRS_EV_POSTGRESQL_REQUEST_H_

#include "ev/request.h"

#include <string> // std::string

namespace ev
{
    namespace postgresql
    {
        
        class Request : public ev::Request
        {
            
        private: // Data
            
            std::string payload_; //!< Request query.
            
        public: // Constructor(s) / Destructor
            
            Request(const Loggable::Data& a_loggable_data, const std::string& a_payload);
            Request(const Loggable::Data& a_loggable_data, const std::vector<char>& a_payload);
            Request(const Loggable::Data& a_loggable_data, const char* const a_format, ...) __attribute__((format(printf, 3, 4)));
            Request(const char* a_query, const Loggable::Data& a_loggable_data);
            virtual ~Request();
            
        public: // Inherited Virtual Method(s) / Function(s)
            
            virtual const char*        AsCString () const;
            virtual const std::string& AsString  () const;
            
        public: // STATIC API METHOD(S) / FUNCTION(S)
            
            static void        SQLEscape (const std::string& a_value, std::string& o_value);
            static std::string SQLEscape (const std::string& a_value);
            
        }; // end of class 'Request'
        
    } // end of namespace 'postgresql'
    
} // end of namespace 'ev'

#endif // NRS_EV_POSTGRESQL_REQUEST_H_
