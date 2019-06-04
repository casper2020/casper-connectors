/**
 * @file md5.h
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_UTILS_MD5_H_
#define NRS_CC_UTILS_MD5_H_

#include <sys/types.h> // int64_t
#include <stdint.h>    // uint8_t, etc
#include <string>

#include <openssl/md5.h>

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

namespace cc
{
    
    namespace utils
    {
        
        class MD5 final : public NonCopyable, NonMovable
        {
            
        private: // Data
            
            unsigned char digest_ [MD5_DIGEST_LENGTH];
            char          md5_hex_[33];
            
        private: // Data

            MD5_CTX context_;
            
        public: // Constructor(s) / Destructor
            
            MD5();
            virtual ~MD5();
            
        public: // Method(s) / Function(s)
            
            void        Initialize ();
            void        Update     (const unsigned char* const a_data, size_t a_length);
            std::string Finalize   ();
            
        }; // end of class 'Time'
        
        
    } // end of namespace 'utils'

} // end of namespace 'cc'

#endif // NRS_CC_UTILS_MD5_H_
