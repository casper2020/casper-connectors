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
#include <string>

#include <string.h>   // strlen

#include "ev/object.h"
#include "ev/exception.h"

namespace ev
{
    namespace curl
    {

        class Value final : public ev::Object
        {

        private: // Data

           int         code_;          //!< > 0 HTTP code, < 0 CURL ERROR CODE.
           std::string body_;          //!< Collected text data, nullptr if none.
           int64_t     last_modified_; //!< Last modified date.

        public: // Constructor(s) / Destructor

            Value   (const int a_code, const std::string& a_body);
            virtual ~Value ();

        public: // Method(s) / Function(s)

            int                code              () const;
            const std::string& body              () const;
            void               set_last_modified (int64_t a_timestamp);
            int64_t            last_modified     () const;

        };

        /**
         * @return Readonly access to received code.
         */
        inline int Value::code () const
        {
            return code_;
        }

        /**
         * @return Readonly access to received data.
         */
        inline const std::string& Value::body () const
        {
            return body_;
        }


        /**
         * @return The last modified timestamp.
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

    } // end of namespace 'curl'

} // end of namespace 'ev'

#endif // NRS_EV_CURL_VALUE_H_
