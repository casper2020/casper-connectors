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

#include "cc/hash/sha256.h"

#include "cc/exception.h"

#include "cc/pragmas.h"

CC_DIAGNOSTIC_PUSH()
CC_DIAGNOSTIC_IGNORED("-Wsign-conversion")
CC_DIAGNOSTIC_IGNORED("-Wunused-parameter")

#include "cppcodec/base64_rfc4648.hpp"

CC_DIAGNOSTIC_PUSH()

const unsigned char cc::hash::SHA256::sk_signature_prefix_[]    = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 };
const size_t        cc::hash::SHA256::sk_signature_prefix_size_ = 19;

/**
 * @brief Default constructor.
 */
cc::hash::SHA256::SHA256 ()
{
    digest_[0]  = 0;
    hex_[0] = '\0';
}

/**
 * @brief Destructor.
 */
cc::hash::SHA256::~SHA256 ()
{
    /* empty */
}

/**
 * @brief Initialize context.
 */
void cc::hash::SHA256::Initialize ()
{
    digest_[0]  = 0;
    hex_   [0]  = '\0';
    SHA256_Init(&context_);
}

/**
 * @brief Update.
 *
 * @param a_data   Data.
 * @param a_length Size in bytes.
 */
void cc::hash::SHA256::Update(const unsigned char* const a_data, size_t a_length)
{
    SHA256_Update(&context_, a_data, static_cast<int>(a_length));
}

/**
 * @brief Finalize calculation.
 *
 * @param
 *
 * @return SHA256 digest non-enoded bytes.
 */
const unsigned char* const cc::hash::SHA256::Final ()
{
    SHA256_Final(digest_, &context_);
    return digest_;
}

/**
 * @brief Finalize calculation.
 *
 * @param  a_format 
 *
 * @return SHA256 digest encoded ( hex default, or base64, etc... ).
 */
std::string cc::hash::SHA256::FinalEncoded (const SHA256::OutputFormat a_format)
{
    SHA256_Final(digest_, &context_);
    if ( SHA256::OutputFormat::HEX == a_format ) {
        size_t r = sizeof(hex_) / sizeof(hex_[0]);
        for( size_t idx = 0; idx < SHA256_DIGEST_LENGTH; idx++ ) {
            snprintf(&(hex_[idx*2]), r, "%02x", (unsigned int)digest_[idx]);
            r -=2;
        }
        hex_[CC_HASH_SHA_256_SHA256_DIGEST_HEX_LENGTH - 1] = '\0';
        return std::string(hex_);
    } else if ( a_format == SHA256::OutputFormat::BASE64_RFC4648 ) {
        return cppcodec::base64_rfc4648::encode(digest_, SHA256_DIGEST_LENGTH);
    }
    throw cc::Exception("Requested output format not implemented yet!");
}

/**
 * @brief Calculate SHA256.
 *
 * @param a_data   Data ( string representation ).
 * @param a_format Output format, one of \link SHA256::OutputFormat \link.
 */
std::string cc::hash::SHA256::Calculate (const std::string& a_data, const SHA256::OutputFormat a_format)
{
    ::cc::hash::SHA256 sha256;
    sha256.Initialize();
    sha256.Update(reinterpret_cast<const unsigned char*>(a_data.c_str()), a_data.size());
    return sha256.FinalEncoded(a_format);
}
