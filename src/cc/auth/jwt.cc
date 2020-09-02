/**
 * @file jwt.cc
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

#include "cc/auth/jwt.h"

#include "cc/auth/exception.h"

#include "cc/crypto/rsa.h"

#include "osal/osalite.h"
#include "osal/osal_time.h"

#include "cc/b64.h"

#include <string.h> // strchr
#include <algorithm> // std::transform

#ifdef __APPLE__
#pragma mark -
#endif

const std::map<cc::auth::JWT::RegisteredClaim, const char* const> cc::auth::JWT::k_registered_claims_ = {
    { cc::auth::JWT::RegisteredClaim::ISS ,"iss" },
    { cc::auth::JWT::RegisteredClaim::SUB ,"sub" },
    { cc::auth::JWT::RegisteredClaim::AUD ,"aud" },
    { cc::auth::JWT::RegisteredClaim::EXP ,"exp" },
    { cc::auth::JWT::RegisteredClaim::NBF ,"nbf" },
    { cc::auth::JWT::RegisteredClaim::IAT ,"iat" },
    { cc::auth::JWT::RegisteredClaim::JTI ,"jti" }
};

const std::map<const char* const, cc::auth::JWT::RegisteredClaim> cc::auth::JWT::k_registered_claims_r_ ={
    { "iss", cc::auth::JWT::RegisteredClaim::ISS },
    { "sub", cc::auth::JWT::RegisteredClaim::SUB },
    { "aud", cc::auth::JWT::RegisteredClaim::AUD },
    { "exp", cc::auth::JWT::RegisteredClaim::EXP },
    { "nbf", cc::auth::JWT::RegisteredClaim::NBF },
    { "iat", cc::auth::JWT::RegisteredClaim::IAT },
    { "jti", cc::auth::JWT::RegisteredClaim::JTI },
};

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Invalid token constructor.
 *
 * @param a_issuer
 */
cc::auth::JWT::JWT (const char* const a_issuer)
    : issuer_(a_issuer)
{
    header_   = Json::Value::null;
    payload_  = Json::Value::null;
}

/**
 * @brief Destructor.
 */
cc::auth::JWT::~JWT ()
{
    /* empty */
}

/**
 * @brief Set a registered claim value.
 *
 * @param a_claim
 * @param a_value
 */
void cc::auth::JWT::SetRegisteredClaim (const cc::auth::JWT::RegisteredClaim a_claim, const Json::Value& a_value)
{
    if ( true == payload_.isNull() ) {
        header_ = Json::Value(Json::ValueType::objectValue);
    }
    switch (a_claim) {
        // 4.1.1.  "iss" (Issuer) Claim - OPTIONAL - StringOrURI
        case cc::auth::JWT::RegisteredClaim::ISS:
            if ( true == a_value.isNull() || false ==a_value.isString() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'iss'!");
            }
            payload_["iss"] = a_value;
            break;
        // 4.1.2.  "sub" (Subject) Claim - OPTIONAL - StringOrURI
        case cc::auth::JWT::RegisteredClaim::SUB:
            if ( true == a_value.isNull() || false ==a_value.isString() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'sub'!");
            }
            payload_["sub"] = a_value;
            break;
        case cc::auth::JWT::RegisteredClaim::AUD:
            if ( true == a_value.isNull() || false ==a_value.isString() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'aud'!");
            }
            payload_["aud"] = a_value;
            break;
        case cc::auth::JWT::RegisteredClaim::EXP:
            if ( true == a_value.isNull() || false ==a_value.isNumeric() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'exp'!");
            }
            payload_["exp"] = a_value;
            break;
        case cc::auth::JWT::RegisteredClaim::NBF:
            if ( true == a_value.isNull() || false ==a_value.isNumeric() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'nbf'!");
            }
            payload_["nbf"] = a_value;
            break;
        case cc::auth::JWT::RegisteredClaim::IAT:
            if ( true == a_value.isNull() || false ==a_value.isNumeric() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'iat'!");
            }
            payload_["iat"] = a_value;
            break;
        case cc::auth::JWT::RegisteredClaim::JTI:
            if ( true == a_value.isNull() || false ==a_value.isString() ) {
                throw cc::auth::Exception("Invalid value for JWT registered claim 'JTI'!");
            }
            payload_["jti"] = a_value;
            break;
        default:
            throw cc::auth::Exception("Don't know how to handle registered claim id " + std::to_string((uint8_t)a_claim) + "!");
    }
}

/**
 * @brief Check if a claim is a registered one.
 *
 * @param a_claim
 *
 * @return True if so, false otherwise.
 */
bool cc::auth::JWT::IsRegisteredClaim (const std::string& a_claim) const
{
    std::string claim = a_claim;

    std::transform(claim.begin(), claim.end(), claim.begin(), ::tolower);

    return k_registered_claims_r_.end() != k_registered_claims_r_.find(claim.c_str());
}

/**
 * @return A registered claim value.
 *
 * @param a_claim
 */
Json::Value cc::auth::JWT::GetRegisteredClaim (const cc::auth::JWT::RegisteredClaim a_claim) const
{
    if ( true == payload_.isNull() ) {
        return Json::Value::null;
    }

    const auto it = k_registered_claims_.find(a_claim);
    if ( k_registered_claims_.end() == it ) {
        throw cc::auth::Exception("Unknown registed claim ' " UINT8_FMT "'!",
                                  static_cast<uint8_t>(a_claim)
        );
    }

    return payload_.get(it->second, Json::Value::null);
}

/**
 * @brief Set a registered claim value.
 *
 * @param a_claim
 * @param a_value
 */
void cc::auth::JWT::SetUnregisteredClaim (const std::string& a_claim, const Json::Value& a_value)
{
    if ( k_registered_claims_r_.end() != k_registered_claims_r_.find(a_claim.c_str()) ) {
        throw cc::auth::Exception("Can't set claim '%s' - it's a registed claim!",
                                  a_claim.c_str()
        );
    }
    payload_[a_claim] = a_value;
}

/**
 * @return A unregistered claim value.
 *
 * @param a_claim
 */
Json::Value cc::auth::JWT::GetUnregisteredClaim (const std::string& a_claim)
{
    if ( true == payload_.isNull() ) {
        return Json::Value::null;
    }
    return payload_.get(a_claim, Json::Value::null);
}

/**
 * @brief Sign using the private key.
 *
 * @param a_duration
 * @param a_private_key_pem
 */
std::string cc::auth::JWT::Encode (const uint64_t a_duration, const std::string& a_private_key_pem)
{
    header_        = Json::Value(Json::ValueType::objectValue);
    header_["typ"] = "JWT";   // JWT
    header_["alg"] = "RS256"; // RS256 - RSASSA-PKCS1-v1_5 using SHA-256 | Recommended

    const int64_t iat = osal::Time::GetUTC();
    const int64_t exp = iat + static_cast<int64_t>(a_duration);

    payload_["exp"] = static_cast<Json::Int64>(exp);
    payload_["iat"] = static_cast<Json::Int64>(iat);

    if ( true == payload_.isNull() ) {
        payload_ = Json::Value(Json::ValueType::objectValue);
        payload_["iss"] = issuer_;
        payload_["nbf"] = static_cast<Json::Int64>(iat);
    } else {
        if ( false == payload_.isMember("iss") ) {
            payload_["iss"] = issuer_;
        }
        if ( false == payload_.isMember("nbf") ) {
            payload_["nbf"] = static_cast<Json::Int64>(iat);
        }
    }

    // ... first encode header ...
    const std::string header_b64    = cc::base64_url_unpadded::encode(fast_writer_.write(header_));
    // ... now encode payload ...
    const std::string payload_b64   = cc::base64_url_unpadded::encode(fast_writer_.write(payload_));
    // ... generate signature ..
    const std::string signature_b64 = cc::crypto::RSA::SignSHA256(header_b64 + '.' + payload_b64, a_private_key_pem);

    // ... finally concat token elements ...
    return header_b64 + "." + payload_b64 + "." + signature_b64;
}

/**
 * @brief Decode a token using a public key.
 *
 * @param a_token
 * @param a_public_key_pem
 */
void cc::auth::JWT::Decode (const std::string& a_token, const std::string& a_public_key_pem)
{
    const char* header_ptr = a_token.c_str();
    if ( '\0' == header_ptr[0] ) {
        throw cc::auth::Exception("Invalid token format - empty header!");
    }

    const char* payload_ptr = strchr(header_ptr, '.');
    if ( nullptr == payload_ptr ) {
        throw cc::auth::Exception("Invalid token format - missing or invalid payload!");
    }
    payload_ptr += sizeof(char);

    const char* signature_ptr = strchr(payload_ptr, '.');
    if ( nullptr == signature_ptr ) {
        throw cc::auth::Exception("Invalid token format - missing or invalid signature!");
    }
    signature_ptr += sizeof(char);

    header_   = Json::nullValue;
    payload_  = Json::nullValue;

    try {

        const std::string header_b64    = std::string(header_ptr , payload_ptr - header_ptr - sizeof(char));
        const std::string payload_b64   = std::string(payload_ptr, signature_ptr - payload_ptr - sizeof(char));
        const std::string signature_b64 = std::string(signature_ptr);

        //
        // LOAD & VERIFY HEADER
        //

        // ... first decode header ...
        const std::string header_json = cc::base64_url_unpadded::decode<std::string>(header_b64.c_str(), header_b64.length());

        // ... now parse header ...
        if ( false == reader_.parse(header_json, header_, false) ) {
            throw cc::auth::Exception("Error while parsing JWT header!");
        }

        // ... validate 'typ' param ...
        const Json::Value typ = header_.get("typ", Json::Value::null);
        if ( true == typ.isNull() || false == typ.isString() ) {
            throw cc::auth::Exception("Unsupported token header param 'typ' - invalid type: got %d, expected %d!",
                                      typ.type(), Json::ValueType::stringValue
            );
        } else if ( 0 != typ.asString().compare("JWT") ) {
            throw cc::auth::Exception("Unsupported token header param 'typ' value!");
        }

        // ... validate 'alg' param ...
        const Json::Value alg = header_.get("alg", Json::Value::null);
        if ( true == alg.isNull() || false == alg.isString() ) {
            throw cc::auth::Exception("Unsupported token header param 'alg' - invalid type: got %d, expected %d!",
                                      alg.type(), Json::ValueType::stringValue
            );
        } else if ( 0 != alg.asString().compare("RS256") ) {
            throw cc::auth::Exception("Unsupported token header param 'alg' value!");
        }

        //
        // VERIFY SIGNATURE
        //
        cc::crypto::RSA::VerifySHA256(header_b64 + '.' + payload_b64, signature_b64, a_public_key_pem);

        //
        // LOAD PAYLOAD
        //

        // ... first decode payload ...
        const std::string payload_json = cc::base64_url_unpadded::decode<std::string>(payload_b64.c_str(), payload_b64.length());

        // ... now parse payload ...
        if ( false == reader_.parse(payload_json, payload_, false) ) {
            throw cc::auth::Exception("Error while parsing JWT payload!");
        }

    } catch (const cppcodec::symbol_error& a_b64_symbol_error) {
        // ... reset ...
        header_  = Json::nullValue;
        payload_ = Json::nullValue;
        // ... rethrow exception ...
        throw cc::auth::Exception("%s", a_b64_symbol_error.what());
    } catch (const cc::auth::Exception& a_broker_exception) {
        // ... reset ...
        header_  = Json::nullValue;
        payload_ = Json::nullValue;
        // ... rethrow exception ...
        throw a_broker_exception;
    }

}

// MARK: -

/**
 * @brief If the JWT had three dots it means a file extension was added to make the browsers fell happy.
 *        To make a valid JWT we need to get rid of the extension making the browser unhappy again.
 *
 * @param a_jwt JWT or kind of a JWT ( when it's keeping the browser 'happy' )...
 */
std::string cc::auth::JWT::MakeBrowsersUnhappy (const std::string& a_jwt)
{
    int dot_count = 0;
    const char* last_dot = nullptr;
    const char* ptr;
    for ( ptr = a_jwt.c_str(); *ptr; ++ptr) {
        if ( '.' == *ptr ) {
            last_dot = ptr;
            dot_count++;
        }
    }
    if ( 3 == dot_count && nullptr != last_dot ) {
        return a_jwt.substr(0, a_jwt.size() - (ptr - last_dot));
    } else {
        return a_jwt;
    }
}

