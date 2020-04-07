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

#include "cc/crypto/hmac.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>

#include "cc/b64.h"

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_key
 */
std::string cc::crypto::HMAC::SHA256 (const std::string& a_payload, const std::string& a_key)
{
    // Initialize HMAC object.
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    HMAC_CTX _ctx;
    HMAC_CTX_init(&ctx);
    HMAC_CTX* ctx = &_ctx;
#else
    HMAC_CTX* ctx = HMAC_CTX_new();
#endif
    
    //
    // int HMAC_Init_ex(HMAC_CTX *ctx, const void *key, int len, const EVP_MD *md, ENGINE *impl)
    //
    HMAC_Init_ex(ctx, a_key.c_str(), static_cast<int>(a_key.length()), EVP_sha256(), NULL);
    
    //
    // int HMAC_Update(HMAC_CTX *ctx, const unsigned char *data, size_t len);
    //
    HMAC_Update(ctx, reinterpret_cast<const unsigned char *>(a_payload.c_str()), a_payload.length());
    
    //
    // int HMAC_Final(HMAC_CTX *ctx, unsigned char *md, unsigned int *len);
    //
    unsigned char buffer[EVP_MAX_MD_SIZE];
    unsigned int  len;
    HMAC_Final(ctx, buffer, &len);
    
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    //
    // void HMAC_CTX_cleanup(HMAC_CTX *ctx);
    //
    HMAC_CTX_cleanup(ctx_ptr);
#else
    HMAC_CTX_free(ctx);
#endif
    
    // convert to base64
    return ::cc::base64_url_unpadded::encode<std::string>(buffer, len);
}
