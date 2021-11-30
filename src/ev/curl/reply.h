/**
 * @file reply.h
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
#ifndef NRS_EV_CURL_REPLY_H_
#define NRS_EV_CURL_REPLY_H_

#include "ev/curl/object.h"
#include "ev/curl/value.h"

#include "curl/curl.h" // CURL

#include <string>

namespace ev
{

    namespace curl
    {

        class Reply final : public ev::curl::Object
        {
            
        private: // Data

            Value value_;

        public: // Constructor(s) / Destructor

            Reply (const int a_code,
                   const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body, size_t a_rtt);
            virtual ~Reply();

        public: // Method(s) / Function(s)

            const Value& value () const;

        public: // Method(s) / Function(s)
            
            void SetInfo (const CURL* a_handle);
            
        }; // end of class 'Reply'

        /**
         * @return Read-only access to collected value.
         */
        inline const Value& Reply::value () const
        {
            return value_;
        }

    }

}

#endif // NRS_EV_CURL_REPLY_H_
