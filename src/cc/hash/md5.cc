/**
 * @file xattr.h
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
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

#include "cc/hash/md5.h"

/**
 * @brief Default constructor.
 */
cc::hash::MD5::MD5 ()
{
    digest_[0]  = 0;
    md5_hex_[0] = '\0';
}

/**
 * @brief Destructor.
 */
cc::hash::MD5::~MD5 ()
{
    /* empty */
}

/**
 * @brief Initialize MD5 context.
 */
void cc::hash::MD5::Initialize ()
{
    digest_[0]  = 0;
    md5_hex_[0] = '\0';
    MD5_Init(&context_);
}

/**
 * @brief Update MD5.
 *
 * @param a_data   Data.
 * @param a_length Size in bytes.
 */
void cc::hash::MD5::Update(const unsigned char* const a_data, size_t a_length)
{
    MD5_Update(&context_, a_data, a_length);
}

/**
 * @brief Finalize and calculate MD5.
 *
 * @return MD5 HEX string.
 */
std::string cc::hash::MD5::Finalize ()
{    
    MD5_Final (digest_,&context_);
    
    for( size_t idx = 0; idx < MD5_DIGEST_LENGTH; idx++ ) {
        sprintf(&(md5_hex_[idx*2]), "%02x", (unsigned int)digest_[idx]);
    }
    md5_hex_[32] = '\0';
    
    return std::string(md5_hex_);
}
