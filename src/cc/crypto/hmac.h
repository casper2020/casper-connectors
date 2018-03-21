/**
 * @file rsa.h
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
#ifndef NRS_CC_CRYPTO_HMAC_H_
#define NRS_CC_CRYPTO_HMAC_H_

#include <string>

#include <openssl/evp.h>

namespace cc
{
    
    namespace crypto
    {
        
        class HMAC final
        {
            
        public: // Static Helpers
            
            static std::string SHA256 (const std::string& a_payload, const std::string& a_secret);
            
        }; // end of class 'HMAC'
        
    } // end of namespace 'crypto'

} // end of namespace 'cc'


#endif // NRS_CC_CRYPTO_HMAC_H_
