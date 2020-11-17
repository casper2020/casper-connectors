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

#include "cppcodec/base64_rfc4648.hpp"

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
 * @brief Initialize MD5 context.
 */
void cc::hash::SHA256::Initialize ()
{
    digest_[0]  = 0;
    hex_   [0]  = '\0';
    SHA256_Init(&context_);
}

/**
 * @brief Update MD5.
 *
 * @param a_data   Data.
 * @param a_length Size in bytes.
 */
void cc::hash::SHA256::Update(const unsigned char* const a_data, size_t a_length)
{
    SHA256_Update(&context_, a_data, static_cast<int>(a_length));
}

/**
 * @brief Finalize and calculate MD5.
 *
 * @param  a_format 
 *
 * @return SHA256 string ( hex default, or base64, etc... ).
 */
std::string cc::hash::SHA256::Finalize (const SHA256::OutputFormat a_format)
{
    SHA256_Final(digest_,&context_);
    if ( SHA256::OutputFormat::HEX == a_format ) {
        for( size_t idx = 0; idx < SHA256_DIGEST_LENGTH; idx++ ) {
            sprintf(&(hex_[idx*2]), "%02x", (unsigned int)digest_[idx]);
        }
        hex_[CC_HASH_SHA_256_SHA256_DIGEST_HEX_LENGTH - 1] = '\0';
        return std::string(hex_);
    } else if ( a_format == SHA256::OutputFormat::BASE64_RFC4648 ) {
        return cppcodec::base64_rfc4648::encode(digest_, SHA256_DIGEST_LENGTH);
    }
    throw cc::Exception("Requested output format not implemented yet!");
}
