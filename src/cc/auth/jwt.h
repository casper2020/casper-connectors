/**
 * @file jwt.h
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
#ifndef NRS_CC_AUTH_JWT_H_
#define NRS_CC_AUTH_JWT_H_

#include <string> // std::string

#include "json/json.h"

namespace cc
{

    namespace auth
    {
        //
        // JWTs generally have three parts:
        //
        // - a header    - identifies which algorithm is used to generate the signature
        //               - ex: {"alg":"HS256","typ":"JWT"}
        // - a payload   - contains the claims that we wish to make
        //               - {"iat":1422779638}
        // - a signature - calculated by base64url encoding the header and payload and concatenating them with a period as a separator
        //
        // for more info, see https://tools.ietf.org/html/rfc7519
        
        class JWT final
        {
            
        public: // Enum(s)
            
            enum class RegisteredClaim : uint8_t
            {
                ISS, // 4.1.1.  "iss" (Issuer) Claim          - OPTIONAL - StringOrURI
                SUB, // 4.1.2.  "sub" (Subject) Claim         - OPTIONAL - StringOrURI
                AUD, // 4.1.3.  "aud" (Audience) Claim        - OPTIONAL - StringOrURI
                EXP, // 4.1.4.  "exp" (Expiration Time) Claim - REQUIRED - NumericDate
                NBF, // 4.1.5.  "nbf" (Not Before) Claim      - OPTIONAL - NumericDate
                IAT, // 4.1.6.  "iat" (Issued At) Claim       - OPTIONAL - NumericDate
                JTI  // 4.1.7.  "jti" (JWT ID) Claim          - OPTIONAL -
            };
            
        public: // Static Const Data
            
            static const std::map<RegisteredClaim, const char* const> k_registered_claims_;
            static const std::map<const char* const, RegisteredClaim> k_registered_claims_r_;
            
        public: // Const Data
            
            const std::string issuer_;
            
        private: // Data
            
            Json::Value      header_;
            Json::Value      payload_;
            Json::FastWriter fast_writer_;
            Json::Reader     reader_;
            
        public: // Constructor(s) / Destructor
            
            JWT (const char* const a_issuer);
            virtual ~JWT ();
            
        public: // Method(s) / Function(s)
            
            bool        IsRegisteredClaim    (const std::string& a_claim) const;
            void        SetRegisteredClaim   (const RegisteredClaim a_claim, const Json::Value& a_value);
            Json::Value GetRegisteredClaim   (const RegisteredClaim a_claim) const;
            
            void        SetUnregisteredClaim (const std::string& a_claim, const Json::Value& a_value);
            Json::Value GetUnregisteredClaim (const std::string& a_claim);
            
            std::string Encode               (const uint64_t a_duration, const std::string& a_private_key_pem);
            void        Decode               (const std::string& a_token, const std::string& a_public_key_pem);
            
            void        Reset                ();
            
            Json::Value Debug                () const;
            
        }; // end of class 'JWT'
        
        /**
         * @brief Reset all data.
         */
        inline void JWT::Reset ()
        {
            header_   = Json::Value::null;
            payload_  = Json::Value::null;
        }
        
        /**
         * @return A 'debug' object.
         */
        inline Json::Value JWT::Debug () const
        {
            Json::Value object = Json::Value(Json::ValueType::objectValue);
            
            object["header"]  = header_;
            object["payload"] = payload_;
            
            return object;
        }
        
    } // end of namespace 'auth'

} // end of namespace 'cc'

#endif // NRS_CC_AUTH_JWT_H_
