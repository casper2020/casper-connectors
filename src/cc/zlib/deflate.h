/**
 * @file deflate.h
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
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
#ifndef NRS_CC_ZLIB_DEFLATE_H_
#define NRS_CC_ZLIB_DEFLATE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "zlib.h"

#include <string>
#include <functional>

namespace cc
{

    namespace zlib
    {

        class Deflate final : public ::cc::NonCopyable, public ::cc::NonMovable
        {
            
        private: // Data
            
            unsigned char* chunk_;
            z_stream*      stream_;
            int            error_;

        public: // Constructor(s) / Destructor

            Deflate ();
            virtual ~Deflate();

        public: // Method(s) / Function(s)

            virtual void Do (const unsigned char* a_data, const size_t a_size,
                             const std::function<void(const unsigned char*, const size_t)>& a_callback,
                             const int8_t a_level = Z_DEFAULT_COMPRESSION,
                             const bool a_gzip = false);
            
        }; // end of class 'Deflate'

    } // end of namespace 'zlib'

} // end of namespace 'cc'

#endif // NRS_CC_ZLIB_DEFLATE_H_
