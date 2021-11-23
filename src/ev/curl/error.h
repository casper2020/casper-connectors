/**
 * @file error.h
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
#ifndef NRS_EV_CURL_ERROR_H_
#define NRS_EV_CURL_ERROR_H_

#include "ev/error.h"

#include "curl/curl.h" // CURLcode

namespace ev
{
    
    namespace curl
    {

        class Error final : public ::ev::Error
        {
            
        public: // Const Data
            
            const CURLcode code_;
            
        public: // Constructor(s) / Destructor
            
            Error(const std::string& a_message);
            Error (const CURLcode a_code, const std::string& a_message);
            Error(const char* const a_format, ...) __attribute__((format(printf, 2, 3)));
            virtual ~Error ();
            
        }; // end of class 'Error'

    } // end of namespace 'curl'
    
} // end of namespace error

#endif // NRS_EV_CURL_ERROR_H_
