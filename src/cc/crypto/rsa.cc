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

#include "cc/crypto/rsa.h"

#include "cc/crypto/exception.h"

#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/err.h>

//
//char err[130] = {0};
//ERR_load_RSA_strings();
//ERR_error_string(ERR_get_error(), err);
//

#include "cc/b64.h"

#ifndef __APPLE__ // backtrace
  #include <stdio.h>
  #include <execinfo.h>
  #include <signal.h>
  #include <stdlib.h>
  #include <unistd.h>
  #include <typeinfo>
#else
  #include <execinfo.h>
  #include <stdio.h>
#endif
#include <string.h> // memcpy, strlen

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_output_format
 *
 * @return
 */
std::string cc::crypto::RSA::SignSHA256 (const std::string& a_payload, const std::string& a_pem,
                                         const RSA::SignOutputFormat a_output_format)
{
    return cc::crypto::RSA::Sign(a_payload, a_pem, /* a_password */ "", EVP_sha256(), RSA::Origin::File, a_output_format);
}

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 * @param a_output_format
 *
 * @return
 */
std::string cc::crypto::RSA::SignSHA256 (const std::string& a_payload, const std::string& a_pem, const std::string& a_password,
                                         const RSA::SignOutputFormat a_output_format)
{
    return cc::crypto::RSA::Sign(a_payload, a_pem, a_password, EVP_sha256(), RSA::Origin::File, a_output_format);
}

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 * @param a_output_format
 *
 * @return
 */
std::string cc::crypto::RSA::SignSHA256 (const unsigned char* a_payload, const size_t a_length, const std::string& a_pem, const std::string& a_password,
                                         const RSA::SignOutputFormat a_output_format)
{
    return cc::crypto::RSA::Sign(a_payload, a_length, a_pem, a_password, EVP_sha256(), RSA::Origin::File, a_output_format);
}

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 * @param a_output_format
 *
 * @return
 */
std::string cc::crypto::RSA::SignSHA256InMemory (const std::string& a_payload, const std::string& a_pem, const std::string& a_password,
                                                 const RSA::SignOutputFormat a_output_format)
{
    return cc::crypto::RSA::Sign(a_payload, a_pem, a_password, EVP_sha256(), RSA::Origin::Memory, a_output_format);
}

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 * @param a_output_format
 *
 * @return
 */
std::string cc::crypto::RSA::SignSHA256InMemory (const unsigned char* a_payload, const size_t a_length, const std::string& a_pem, const std::string& a_password,
                                                 const RSA::SignOutputFormat a_output_format)
{
    return cc::crypto::RSA::Sign(a_payload, a_length, a_pem, a_password, EVP_sha256(), RSA::Origin::Memory, a_output_format);
}

/**
 * @brief Verify using the public key.
 *
 * @param a_pem
 * @param a_payload
 * @param a_signature
 */
void cc::crypto::RSA::VerifySHA256 (const std::string& a_payload, const std::string& a_signature, const std::string& a_pem)
{
    cc::crypto::RSA::Verify(a_payload, a_signature, a_pem, EVP_sha256());
}

/**
 * @brief Encrypt using the public key.
 *
 * @param a_payload
 * @param a_pem
 */
std::string cc::crypto::RSA::PublicKeyEncrypt (const std::string& a_payload, const std::string& a_pem)
{
    EVP_PKEY*       pkey             = nullptr;
    struct rsa_st*  rsa_pkey         = nullptr;
    FILE*           public_key_file  = nullptr;
    unsigned char* out               = nullptr;
    int            outl              = 0;
    
    const auto cleanup = [&pkey, &public_key_file, &out] () {
        
        if ( nullptr != out ) {
            delete [] out;
        }
        
        if ( nullptr != pkey ) {
            EVP_PKEY_free(pkey);
        }
        
        if ( nullptr != public_key_file ) {
            fclose(public_key_file);
        }
        
    };
    
    try {
        
        //
        // Load public key.
        //
        pkey            = EVP_PKEY_new();
        public_key_file = fopen(a_pem.c_str(), "r");
        if ( nullptr == public_key_file ) {
            throw ::cc::crypto::Exception("Unable to open RSA public key file!");
        }
        
        //
        // RSA *PEM_read_RSAPublicKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
        // RSA *PEM_read_RSA_PUBKEY(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
        
        //
        // PEM_read_RSA_PUBKEY() reads the PEM format. PEM_read_RSAPublicKey() reads the PKCS#1 format.
        //
        // Returns RSA* for success and NULL for failure.
        //
        const struct rsa_st* rsa;
        if ( ! ( rsa = PEM_read_RSA_PUBKEY(public_key_file, &rsa_pkey, NULL, NULL) ) ) {
            throw ::cc::crypto::Exception("Error while loading RSA public key File!");
        }
        
        out = new unsigned char [RSA_size(rsa_pkey)];
        
        //
        //  int RSA_public_encrypt(int flen, unsigned char *from, unsigned char *to, RSA *rsa, int padding)
        //
        // - encrypts the flen bytes at from (usually a session key) using the public key rsa and stores the ciphertext in to. to must point to RSA_size(rsa) bytes of memory.
        //
        // Returns the size of the encrypted data, -1 on error
        //
        if ( ( outl = RSA_public_encrypt(static_cast<int>(a_payload.length()), reinterpret_cast<const unsigned char*>(a_payload.c_str()), out, rsa_pkey, RSA_PKCS1_PADDING) ) < 0 ) {
            throw ::cc::crypto::Exception("Error while encrypting data with RSA public key!");
        }
        
        const std::string b64 = ::cc::base64_rfc4648::encode(out, static_cast<size_t>(outl));
        
        cleanup();
        
        return b64;
        
    } catch (const ::cc::crypto::Exception& a_crypto_exception) {
        cleanup();
        throw a_crypto_exception;
    }  catch (const std::bad_alloc& a_bad_alloc) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Bad Alloc: %s", a_bad_alloc.what());
    } catch (const std::runtime_error& a_rte) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Runtime Error: %s", a_rte.what());
    } catch (const std::exception& a_std_exception) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Standard Exception: %s", a_std_exception.what());
    } catch (...) {
        cleanup();
        std::exception_ptr p = std::current_exception();
        if ( p ) {
#ifdef __APPLE__
            try {
                std::rethrow_exception(p);
            } catch(const std::exception& e) {
                throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                              e.what()
                );
            }
#else
            throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                          p.__cxa_exception_type()->name()
            );
#endif
        } else {
            throw ::cc::crypto::Exception("C++ Generic Exception!");
        }
    }
}

/**
 * @brief Encrypt using the public key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 */
std::string cc::crypto::RSA::PrivateKeyDecrypt (const std::string& a_payload, const std::string& a_pem, const std::string& a_password)
{
    EVP_PKEY*      pkey             = nullptr;
    struct rsa_st* rsa_pkey         = nullptr;
    FILE*          private_key_file = nullptr;
    unsigned char* out              = nullptr;
    int            outl             = 0;
    size_t         payload_len      = 0;
    unsigned char* payload_bytes    = nullptr;
    
    const auto cleanup = [&pkey, &private_key_file, &out, &payload_bytes] () {
        
        if ( nullptr != out ) {
            delete [] out;
        }
        
        if ( nullptr != pkey ) {
            EVP_PKEY_free(pkey);
        }
        
        if ( nullptr != private_key_file ) {
            fclose(private_key_file);
        }
        
        if ( nullptr != payload_bytes ) {
            delete [] payload_bytes;
        }
        
    };
    
    try {
        
        //
        // Load public key.
        //
        pkey            = EVP_PKEY_new();
        private_key_file = fopen(a_pem.c_str(), "r");
        if ( nullptr == private_key_file ) {
            throw ::cc::crypto::Exception("Unable to open RSA public key file!");
        }
        
        //
        // RSA *PEM_read_RSAPrivateKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
        //
        // Returns RSA* for success and NULL for failure.
        //
        const struct rsa_st* rsa;
        if ( ! ( rsa = PEM_read_RSAPrivateKey(private_key_file, &rsa_pkey, &pem_password_cb, (void*)a_password.c_str()) ) ) {
            throw ::cc::crypto::Exception("Error while loading RSA private key file!");
        }
        
        payload_len   = ::cc::base64_rfc4648::decoded_max_size(a_payload.length());
        payload_bytes = new unsigned char[payload_len];
        payload_len   = ::cc::base64_rfc4648::decode(payload_bytes, payload_len, a_payload.c_str(), a_payload.length());
        
        out = new unsigned char [RSA_size(rsa_pkey)];
        
        //
        //  int RSA_public_decrypt(int flen, unsigned char *from, unsigned char *to, RSA *rsa, int padding)
        //
        // - encrypts the flen bytes at from (usually a session key) using the public key rsa and stores the ciphertext in to. to must point to RSA_size(rsa) bytes of memory.
        //
        // Returns the size of the encrypted data, -1 on error
        //
        if ( ( outl = RSA_private_decrypt(static_cast<int>(payload_len), payload_bytes, out, rsa_pkey, RSA_PKCS1_PADDING) ) < 0 ) {
            throw ::cc::crypto::Exception("Error while encrypting data with RSA public key!");
        }
        
        const std::string decoded = std::string(reinterpret_cast<const char*>(out), static_cast<size_t>(outl));
        
        cleanup();
        
        return decoded;
        
    } catch (const ::cc::crypto::Exception& a_crypto_exception) {
        cleanup();
        throw a_crypto_exception;
    }  catch (const std::bad_alloc& a_bad_alloc) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Bad Alloc: %s", a_bad_alloc.what());
    } catch (const std::runtime_error& a_rte) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Runtime Error: %s", a_rte.what());
    } catch (const std::exception& a_std_exception) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Standard Exception: %s", a_std_exception.what());
    } catch (...) {
        cleanup();
        std::exception_ptr p = std::current_exception();
        if ( p ) {
#ifdef __APPLE__
            try {
                std::rethrow_exception(p);
            } catch(const std::exception& e) {
                throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                              e.what()
                );
            }
#else
            throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                          p.__cxa_exception_type()->name()
            );
#endif
        } else {
            throw ::cc::crypto::Exception("C++ Generic Exception!");
        }
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 * @param a_evp_md
 * @param a_origin
 * @param a_output_format
 */
std::string cc::crypto::RSA::Sign (const std::string& a_payload, const std::string& a_pem, const std::string a_password, const EVP_MD* a_evp_md,
                                   const RSA::Origin a_origin, const RSA::SignOutputFormat a_output_format)
{
    bool           ctx_initialized  = false;
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    EVP_MD_CTX    _ctx;
    EVP_MD_CTX*    ctx = &_ctx;
#else
    EVP_MD_CTX*    ctx = nullptr;
#endif
    EVP_PKEY*      pkey             = nullptr;
    struct rsa_st* rsa_pkey         = nullptr;
    FILE*          private_key_file = nullptr;
    BIO*           pkey_bio         = nullptr;
    unsigned char* signature_bytes  = nullptr;
    unsigned int   signature_len    = 0;

    
    const auto cleanup = [&pkey, &private_key_file, &pkey_bio, &signature_bytes, ctx, &ctx_initialized] () {
        
        if ( nullptr != signature_bytes ) {
            delete [] signature_bytes;
        }
        
        if ( nullptr != pkey ) {
            EVP_PKEY_free(pkey);
        }
        
        if ( nullptr != private_key_file ) {
            fclose(private_key_file);
        }
        
        if ( nullptr != pkey_bio ) {
            BIO_free(pkey_bio);
        }
        
        if ( true == ctx_initialized ) {
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
            EVP_MD_CTX_cleanup(ctx);
#else
            EVP_MD_CTX_free(ctx);
#endif
            ctx_initialized = false;
        }
        
    };
    
    try {
        
        //
        // Load private key.
        //
        pkey = EVP_PKEY_new();
        // ...
        if ( RSA::Origin::File == a_origin ) {
            // ... load private key from file ...
            private_key_file = fopen(a_pem.c_str(), "r");
            if ( nullptr == private_key_file ) {
                throw ::cc::crypto::Exception("Unable to open RSA private key file!");
            }
            
            //
            // RSA *PEM_read_RSAPrivateKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
            //
            // - The PEM functions read or write structures in PEM format.
            //
            // Returns RSA* for success and NULL for failure
            if ( 0 != a_password.length() ) {
                const struct rsa_st* rsa;
                if ( ! ( rsa = PEM_read_RSAPrivateKey(private_key_file, &rsa_pkey, &pem_password_cb, (void*)a_password.c_str()) ) ) {
                    throw ::cc::crypto::Exception("Error while loading RSA private key file!");
                }
            } else {
                if ( ! PEM_read_RSAPrivateKey(private_key_file, &rsa_pkey, NULL, NULL) ) {
                    throw ::cc::crypto::Exception("Error while loading RSA private key file!");
                }
            }
        } else if ( RSA::Origin::Memory == a_origin ) {
            // ... load private key from memory ...
            pkey_bio = BIO_new_mem_buf((void*)a_pem.c_str(), static_cast<int>(a_pem.length()));
            //
            // PEM_read_bio_RSAPrivateKey(BIO *bp, RSA **x, pem_password_cb *cb, void *u)
            //
            // Returns RSA* for success and NULL for failure
            if ( ! PEM_read_bio_RSAPrivateKey(pkey_bio, &rsa_pkey, &pem_password_cb, (void*)a_password.c_str()) ) {
                throw ::cc::crypto::Exception("Error while loading RSA private key!");
            }
        } else {
            throw ::cc::crypto::Exception("Dont know how to load RSA private key!");
        }

        //
        // EVP_PKEY_assign_RSA
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_PKEY_assign_RSA(pkey, rsa_pkey) ) {
            throw ::cc::crypto::Exception("Error while assigning RSA!");
        }
        
        //
        // Initializes digest context ctx.
        //
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
        EVP_MD_CTX_init(ctx);
#else
        ctx = EVP_MD_CTX_new();
#endif
        ctx_initialized = true;
        
        //
        // int EVP_SignInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
        //
        // - Sets up signing context ctx to use digest type from ENGINE impl.
        // - ctx must be created with EVP_MD_CTX_new() before calling this function.
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_SignInit(ctx, a_evp_md) ) {
            throw ::cc::crypto::Exception("Error while setting up signing context!");
        }
        
        //
        // int EVP_SignUpdate(EVP_MD_CTX *ctx, const void *d, unsigned int cnt);
        //
        // - Hashes cnt bytes of data at d into the signature context ctx.
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_SignUpdate(ctx, a_payload.c_str(), a_payload.length() ) ) {
            throw ::cc::crypto::Exception("Error while updating signing context");
        }
        
        //
        // int EVP_SignFinal(EVP_MD_CTX *ctx, unsigned char *sig, unsigned int *s, EVP_PKEY *pkey);
        //
        // - Signs the data in ctx using the private key pkey and places the signature in sig.
        //
        // Returns 1 for success and 0 for failure.
        //
        signature_bytes = new unsigned char[EVP_PKEY_size(pkey)];
        if ( 1 != EVP_SignFinal(ctx, signature_bytes, &signature_len, pkey) ) {
            throw ::cc::crypto::Exception("Error while finalizing signing context!");
        }
        
        //
        // BASE64 encode
        //
        std::string signature_b64;
        if ( RSA::SignOutputFormat::BASE64_RFC4648 == a_output_format ) {
            signature_b64 = ::cc::base64_rfc4648::encode(signature_bytes, static_cast<size_t>(signature_len));
        } else if ( RSA::SignOutputFormat::BASE64_URL_UNPADDED == a_output_format ) {
            signature_b64 = ::cc::base64_url_unpadded::encode(signature_bytes, static_cast<size_t>(signature_len));
        } else {
            throw ::cc::crypto::Exception("BASE64 encoded %u - not implemented!", static_cast<unsigned>(a_output_format));
        }
    
        cleanup();
        
        return signature_b64;
        
    } catch (const ::cc::crypto::Exception& a_crypto_exception) {
        cleanup();
        throw a_crypto_exception;
    }  catch (const std::bad_alloc& a_bad_alloc) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Bad Alloc: %s", a_bad_alloc.what());
    } catch (const std::runtime_error& a_rte) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Runtime Error: %s", a_rte.what());
    } catch (const std::exception& a_std_exception) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Standard Exception: %s", a_std_exception.what());
    } catch (...) {
        cleanup();
        std::exception_ptr p = std::current_exception();
        if ( p ) {
#ifdef __APPLE__
            try {
                std::rethrow_exception(p);
            } catch(const std::exception& e) {
                throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                              e.what()
                );
            }
#else
            throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                      p.__cxa_exception_type()->name()
            );
#endif
        } else {
            throw ::cc::crypto::Exception("C++ Generic Exception!");
        }
    }
}

/**
 * @brief Sign using the private key.
 *
 * @param a_payload
 * @param a_pem
 * @param a_password
 * @param a_evp_md
 * @param a_orign
 * @param a_output_format
 */
std::string cc::crypto::RSA::Sign (const unsigned char* a_payload, const size_t a_length, const std::string& a_pem, const std::string a_password, const EVP_MD* a_evp_md,
                                   const RSA::Origin a_origin, const SignOutputFormat a_output_format)
{
    bool           ctx_initialized  = false;
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    EVP_MD_CTX    _ctx;
    EVP_MD_CTX*    ctx = &_ctx;
#else
    EVP_MD_CTX*    ctx = nullptr;
#endif
    EVP_PKEY*      pkey             = nullptr;
    struct rsa_st* rsa_pkey         = nullptr;
    FILE*          private_key_file = nullptr;
    BIO*           pkey_bio         = nullptr;
    unsigned char* signature_bytes  = nullptr;
    unsigned int   signature_len    = 0;
    
    const auto cleanup = [&pkey, &private_key_file, &pkey_bio, &signature_bytes, ctx, &ctx_initialized] () {
        
        if ( nullptr != signature_bytes ) {
            delete [] signature_bytes;
        }
        
        if ( nullptr != pkey ) {
            EVP_PKEY_free(pkey);
        }
        
        if ( nullptr != private_key_file ) {
            fclose(private_key_file);
        }
        
        if ( nullptr != pkey_bio ) {
            BIO_free(pkey_bio);
        }
        
        if ( true == ctx_initialized ) {
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
            EVP_MD_CTX_cleanup(ctx);
#else
            EVP_MD_CTX_free(ctx);
#endif
            ctx_initialized = false;
        }
        
    };
    
    try {
        
        //
        // Load private key.
        //
        pkey             = EVP_PKEY_new();
        // ...
        if ( RSA::Origin::File == a_origin ) {
            // ... load private key from file ...
            private_key_file = fopen(a_pem.c_str(), "r");
            if ( nullptr == private_key_file ) {
                throw ::cc::crypto::Exception("Unable to open RSA private key file!");
            }
            
            //
            // RSA *PEM_read_RSAPrivateKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
            //
            // - The PEM functions read or write structures in PEM format.
            //
            // Returns RSA* for success and NULL for failure
            if ( 0 != a_password.length() ) {
                const struct rsa_st* rsa;
                if ( ! ( rsa = PEM_read_RSAPrivateKey(private_key_file, &rsa_pkey, &pem_password_cb, (void*)a_password.c_str()) ) ) {
                    throw ::cc::crypto::Exception("Error while loading RSA private key file!");
                }
            } else {
                if ( ! PEM_read_RSAPrivateKey(private_key_file, &rsa_pkey, NULL, NULL) ) {
                    throw ::cc::crypto::Exception("Error while loading RSA private key file!");
                }
            }
        } else if ( RSA::Origin::Memory == a_origin ) {
            // ... load private key from memory ...
            pkey_bio = BIO_new_mem_buf((void*)a_pem.c_str(), static_cast<int>(a_pem.length()));
            //
            // PEM_read_bio_RSAPrivateKey(BIO *bp, RSA **x, pem_password_cb *cb, void *u)
            //
            // Returns RSA* for success and NULL for failure
            if ( ! PEM_read_bio_RSAPrivateKey(pkey_bio, &rsa_pkey, &pem_password_cb, (void*)a_password.c_str()) ) {
                throw ::cc::crypto::Exception("Error while loading RSA private key!");
            }
        } else {
            throw ::cc::crypto::Exception("Dont know how to load RSA private key!");
        }

        //
        // EVP_PKEY_assign_RSA
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_PKEY_assign_RSA(pkey, rsa_pkey) ) {
            throw ::cc::crypto::Exception("Error while assigning RSA!");
        }
        
        //
        // Initializes digest context ctx.
        //
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
        EVP_MD_CTX_init(ctx);
#else
        ctx = EVP_MD_CTX_new();
#endif
        ctx_initialized = true;
        
        //
        // int EVP_SignInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
        //
        // - Sets up signing context ctx to use digest type from ENGINE impl.
        // - ctx must be created with EVP_MD_CTX_new() before calling this function.
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_SignInit(ctx, a_evp_md) ) {
            throw ::cc::crypto::Exception("Error while setting up signing context!");
        }
        
        //
        // int EVP_SignUpdate(EVP_MD_CTX *ctx, const void *d, unsigned int cnt);
        //
        // - Hashes cnt bytes of data at d into the signature context ctx.
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_SignUpdate(ctx, a_payload, a_length) ) {
            throw ::cc::crypto::Exception("Error while updating signing context");
        }
        
        //
        // int EVP_SignFinal(EVP_MD_CTX *ctx, unsigned char *sig, unsigned int *s, EVP_PKEY *pkey);
        //
        // - Signs the data in ctx using the private key pkey and places the signature in sig.
        //
        // Returns 1 for success and 0 for failure.
        //
        signature_bytes = new unsigned char[EVP_PKEY_size(pkey)];
        if ( 1 != EVP_SignFinal(ctx, signature_bytes, &signature_len, pkey) ) {
            throw ::cc::crypto::Exception("Error while finalizing signing context!");
        }
        
        //
        // BASE64 encode
        //
        std::string signature_b64;
        if ( RSA::SignOutputFormat::BASE64_RFC4648 == a_output_format ) {
            signature_b64 = ::cc::base64_rfc4648::encode(signature_bytes, static_cast<size_t>(signature_len));
        } else if ( RSA::SignOutputFormat::BASE64_URL_UNPADDED == a_output_format ) {
            signature_b64 = ::cc::base64_url_unpadded::encode(signature_bytes, static_cast<size_t>(signature_len));
        } else {
            throw ::cc::crypto::Exception("BASE64 encoded %u - not implemented!", static_cast<unsigned>(a_output_format));
        }
    
        cleanup();
        
        return signature_b64;
        
    } catch (const ::cc::crypto::Exception& a_crypto_exception) {
        cleanup();
        throw a_crypto_exception;
    }  catch (const std::bad_alloc& a_bad_alloc) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Bad Alloc: %s", a_bad_alloc.what());
    } catch (const std::runtime_error& a_rte) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Runtime Error: %s", a_rte.what());
    } catch (const std::exception& a_std_exception) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Standard Exception: %s", a_std_exception.what());
    } catch (...) {
        cleanup();
        std::exception_ptr p = std::current_exception();
        if ( p ) {
#ifdef __APPLE__
            try {
                std::rethrow_exception(p);
            } catch(const std::exception& e) {
                throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                              e.what()
                );
            }
#else
            throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                      p.__cxa_exception_type()->name()
            );
#endif
        } else {
            throw ::cc::crypto::Exception("C++ Generic Exception!");
        }
    }
}

/**
 * @brief Verify using the public key.
 *
 * @param a_payload
 * @param a_signature
 * @param a_pem
 * @param a_evp_md
 */
void cc::crypto::RSA::Verify (const std::string& a_payload, const std::string& a_signature, const std::string& a_pem, const EVP_MD* a_evp_md)
{
    bool           ctx_initialized  = false;
    #if OPENSSL_VERSION_NUMBER < 0x1010000fL
        EVP_MD_CTX    _ctx;
        EVP_MD_CTX*    ctx = &_ctx;
    #else
        EVP_MD_CTX*    ctx = nullptr;
    #endif
    EVP_PKEY*      pkey             = nullptr;
    struct rsa_st* rsa_pkey         = nullptr;
    FILE*          public_key_file  = nullptr;
    unsigned char* signature_bytes  = nullptr;
    size_t         signature_len    = 0;
    int            rv               = -1;
    
    const auto cleanup = [&pkey, &public_key_file, &signature_bytes, &ctx_initialized, ctx] () {
        
        if ( nullptr != signature_bytes ) {
            delete [] signature_bytes;
        }
        
        if ( nullptr != pkey ) {
            EVP_PKEY_free(pkey);
        }
        
        if ( nullptr != public_key_file ) {
            fclose(public_key_file);
        }
        
        if ( true == ctx_initialized ) {
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
            EVP_MD_CTX_cleanup(ctx);
#else
            EVP_MD_CTX_free(ctx);
#endif
            ctx_initialized = false;
        }
        
    };
    
    try {
        
        //
        // Decode signature bytes.
        //
        signature_len   = ::cc::base64_url_unpadded::decoded_max_size(a_signature.length());
        signature_bytes = new unsigned char[signature_len];
        
        ::cc::base64_url_unpadded::decode(signature_bytes, signature_len, a_signature.c_str(), a_signature.length());
        
        //
        // Load public key.
        //
        pkey            = EVP_PKEY_new();
        public_key_file = fopen(a_pem.c_str(), "r");
        if ( nullptr == public_key_file ) {
            throw ::cc::crypto::Exception("Unable to open RSA public key file!");
        }
        
        //
        // RSA *PEM_read_RSAPublicKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
        // RSA *PEM_read_RSA_PUBKEY(FILE *fp, RSA **x, pem_password_cb *cb, void *u);

        //
        // PEM_read_RSA_PUBKEY() reads the PEM format. PEM_read_RSAPublicKey() reads the PKCS#1 format.
        //
        // Returns RSA* for success and NULL for failure.
        //
        if ( ! PEM_read_RSA_PUBKEY(public_key_file, &rsa_pkey, NULL, NULL) ) {
            throw ::cc::crypto::Exception("Error while loading RSA public key File!");
        }
        
        //
        // EVP_PKEY_assign_RSA
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_PKEY_assign_RSA(pkey, rsa_pkey) ) {
            throw ::cc::crypto::Exception("Error while assigning RSA!");
        }

        //
        // Initializes digest context ctx.
        //
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
        EVP_MD_CTX_init(ctx);
#else
        ctx = EVP_MD_CTX_new();
#endif
        ctx_initialized = true;
        
        //
        // int EVP_VerifyInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
        //
        // - Sets up verification context ctx to use digest type from ENGINE impl.
        // - ctx must be initialized by calling EVP_MD_CTX_init() before calling this function.
        //
        // Returns 1 for success and 0 for failure.
        //
        if ( 1 != EVP_VerifyInit_ex(ctx, a_evp_md, NULL) ) {
            throw ::cc::crypto::Exception("Error while setting up signing context!");
        }
        
        //
        // int EVP_VerifyUpdate(EVP_MD_CTX *ctx, const void *d, unsigned int cnt);
        //
        // - Hashes cnt bytes of data at d into the verification context ctx.
        // - This function can be called several times on the same ctx to include additional data.
        //
        // Returns 1 for success and 0 for failure.
        //
        const void* data = a_payload.c_str();
        unsigned int cnt = static_cast<unsigned int>(a_payload.length());
        if ( 1 != ( rv = EVP_VerifyUpdate(ctx, data, cnt) ) ) {
            throw ::cc::crypto::Exception("Error while verifying data!");
        }
        
        //
        //
        // int EVP_VerifyFinal(EVP_MD_CTX *ctx,unsigned char *sigbuf, unsigned int siglen,EVP_PKEY *pkey);
        //
        // - Verifies the data in ctx using the public key pkey and against the siglen bytes at sigbuf.
        //
        //  Returns 1 for a correct signature, 0 for failure and -1 if some other error occurred.
        //
        if ( 1 != ( rv = EVP_VerifyFinal(ctx, signature_bytes, static_cast<unsigned int>(signature_len), pkey) ) ) {
            throw ::cc::crypto::Exception("Error while verifying signature!");
        }
        
        cleanup();
        
    } catch (const ::cc::crypto::Exception& a_crypto_exception) {
        cleanup();
        throw a_crypto_exception;
    }  catch (const std::bad_alloc& a_bad_alloc) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Bad Alloc: %s", a_bad_alloc.what());
    } catch (const std::runtime_error& a_rte) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Runtime Error: %s", a_rte.what());
    } catch (const std::exception& a_std_exception) {
        cleanup();
        throw ::cc::crypto::Exception("C++ Standard Exception: %s", a_std_exception.what());
    } catch (...) {
        cleanup();
        std::exception_ptr p = std::current_exception();
        if ( p ) {
#ifdef __APPLE__
            try {
                std::rethrow_exception(p);
            } catch(const std::exception& e) {
                throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                              e.what()
                );
            }
#else
            throw ::cc::crypto::Exception("C++ Generic Exception:, %s",
                                          p.__cxa_exception_type()->name()
            );
#endif
        } else {
            throw ::cc::crypto::Exception("C++ Generic Exception!");
        }
    }
}

/**
 * @brief Password callback.
 *
 * @param a_buffer
 * @param a_size
 * @param a_rw_flag
 * @param a_user_data
 */
int cc::crypto::RSA::pem_password_cb (char* a_buffer, int a_size, int /* a_rw_flag */, void* a_user_data)
{
    const char* const pw     = (const char* const)a_user_data;
    const size_t      pw_len = strlen(pw);
    const size_t      cp_len = pw_len <= (size_t)a_size ? pw_len : (size_t)a_size;
    
    memcpy(a_buffer, pw, cp_len);
    
    return static_cast<int>(cp_len);
}

