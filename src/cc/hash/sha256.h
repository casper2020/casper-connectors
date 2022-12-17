/**
 * @file sha256.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_HASH_SHA256_H_
#define NRS_CC_HASH_SHA256_H_

#include <openssl/sha.h>

#include <string>

#define CC_HASH_SHA_256_SHA256_DIGEST_HEX_LENGTH ( 2 * SHA256_DIGEST_LENGTH ) + 1

namespace cc
{
    
    namespace hash
    {
        
        class SHA256 final // : public NonCopyable, NonMovable
        {
            
        public : // Static Const Data
            
            static const unsigned char sk_signature_prefix_[];    //!< 19 byte ASN1 structure from the IETF rfc3447 for SHA256
            static const size_t        sk_signature_prefix_size_; //!< always 19 bytes
            
        public: // Enum(s)
            
            enum OutputFormat {
                HEX = 0,
                BASE64_RFC4648
            };
            
        private: // Data
            
            unsigned char digest_ [SHA256_DIGEST_LENGTH];
            char          hex_    [CC_HASH_SHA_256_SHA256_DIGEST_HEX_LENGTH];
            
        private: // Data

            SHA256_CTX context_;
            
        public: // Constructor(s) / Destructor
            
            SHA256();
            virtual ~SHA256();
            
        public: // Method(s) / Function(s)
            
            void                       Initialize   ();
            void                       Update       (const unsigned char* const a_data, size_t a_length);
            const unsigned char* const Final        ();
            std::string                FinalEncoded (const OutputFormat a_format = OutputFormat::HEX);
            
        public: // Static Method(s) / Function(s)
            
            static std::string Calculate (const std::string& a_data, const OutputFormat a_format = OutputFormat::HEX);
            
        }; // end of class 'SHA256'
        
    } // end of namespace 'hash'

} // end of namespace 'cc'

#endif // NRS_CC_HASH_SHA256_H_
