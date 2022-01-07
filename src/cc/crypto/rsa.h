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
#ifndef NRS_CC_CRYPTO_RSA_H_
#define NRS_CC_CRYPTO_RSA_H_

#include <string>

#include <openssl/evp.h>

namespace cc
{
    
    namespace crypto
    {
        
        class RSA final
        {
            
        public: // Enum(s)
            
            enum Origin : uint8_t {
                File,
                Memory
            };
            
        public: // Enum(s)
            
            enum SignOutputFormat {
                NotSet = -1,
                BASE64_URL_UNPADDED = 0,
                BASE64_RFC4648
            };
            
        public: // Static Helpers
            
            static std::string SignSHA256        (const std::string& a_payload, const std::string& a_pem,
                                                  const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            static std::string SignSHA256        (const std::string& a_payload, const std::string& a_pem, const std::string& a_password,
                                                  const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            static std::string SignSHA256        (const unsigned char* a_payload, const size_t a_length, const std::string& a_pem, const std::string& a_password,
                                                  const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            
            static std::string SignSHA256InMemory (const std::string& a_payload, const std::string& a_pem, const std::string& a_password,
                                                   const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            static std::string SignSHA256InMemory (const unsigned char* a_payload, const size_t a_length, const std::string& a_pem, const std::string& a_password,
                                                   const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            
            static void        VerifySHA256      (const std::string& a_payload, const std::string& a_signature, const std::string& a_pub);
            
            static std::string PublicKeyEncrypt  (const std::string& a_payload, const std::string& a_pem);
            static std::string PrivateKeyDecrypt (const std::string& a_payload, const std::string& a_pem, const std::string& a_password);
            
        public:
            
        private: // Static Helpers
            
            static std::string Sign    (const std::string& a_payload, const std::string& a_pem, const std::string a_password, const EVP_MD* a_evp_md,
                                        const Origin a_origin = Origin::File,
                                        const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            static std::string Sign    (const unsigned char* a_payload, const size_t a_length, const std::string& a_pem, const std::string a_password, const EVP_MD* a_evp_md,
                                        const Origin a_origin = Origin::File,
                                        const SignOutputFormat a_output_format = SignOutputFormat::BASE64_URL_UNPADDED);
            
            static void        Verify  (const std::string& a_payload, const std::string& a_signature, const std::string& a_pub, const EVP_MD* a_evp_md);
            
            static int         pem_password_cb (char* a_buffer, int a_size, int a_rw_flag, void* a_user_data);
            
        }; // end of class 'RSA'
        
    } // end of namespace 'crypto'
    
} // end of namespace 'cc'

#endif // NRS_CC_CRYPTO_RSA_H_
