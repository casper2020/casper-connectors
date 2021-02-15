/**
 * @file x509.cc
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/crt/x509_reader.h"

#include "cc/exception.h"
#include "cc/macros.h"

#include <chrono>
#include <map>
#include <string.h> // strlen

#include <openssl/pem.h>
#include <openssl/x509v3.h>

/**
 * @brief Default constructor.
 */
cc::crt::X509Reader::X509Reader ()
{
    x509_     = nullptr;
    name_ptr_ = nullptr;
}
    
/**
 * @brief Destructor.
 */
cc::crt::X509Reader::~X509Reader ()
{
    if ( nullptr != x509_ ) {
        X509_free(x509_);
    }
}

/**
 * @brief Load a X509 from a PEM.
 *
 * @param a_pem Certificate in PEM format.
 */
void cc::crt::X509Reader::Load (const std::string& a_pem)
{
    Unload();
    
    //
    // PEM_read_bio_X509(BIO *bp, X509 **x, pem_password_cb *cb, void *u);
    //
    // Returns X509* for success and NULL for failure
    BIO* pem_bio = BIO_new_mem_buf((void*)a_pem.c_str(), static_cast<int>(a_pem.length()));
    if ( ! PEM_read_bio_X509(pem_bio, &x509_, /* pem_password_cb */ nullptr, /* u */ nullptr) ) {
        BIO_free(pem_bio);
        if ( nullptr != x509_ ) {
            X509_free(x509_);
            x509_ = nullptr;
        }
        throw ::cc::Exception("%s", "Unable to load X509 from PEM!");
    }
    BIO_free(pem_bio);
    
    name_ptr_ = X509_get_subject_name(x509_);
}

/**
 * @brief Close / Unload / Release currenly loaded X509.
 */
void cc::crt::X509Reader::Unload ()
{
    if ( nullptr != x509_ ) {
        X509_free(x509_);
    }
    x509_ = nullptr;
}

/**
 * @return True if currenly loaded X509 is CA.
 */
bool cc::crt::X509Reader::IsCA () const
{
    return ( 0 == X509_check_ca(x509_) );
}

/**
 * @brief Read an 'subject DN' value from currenly loaded X509.
 *
 * @param o_value Read 'subject DN' value.
 */
void cc::crt::X509Reader::GetSubjectDN (std::string& o_value) const
{
    char* subject = X509_NAME_oneline(name_ptr_, NULL, 0);
    if ( nullptr != subject ) {
        o_value = subject;
        OPENSSL_free(subject);
    }
}

/**
 * @brief Read an 'subject DN' value from currenly loaded X509.
 *
 * @return Read 'subject DN' value.
 */
std::string cc::crt::X509Reader::GetSubjectDN () const
{
    std::string value;
    char* subject = X509_NAME_oneline(name_ptr_, NULL, 0);
    if ( nullptr != subject ) {
        value = subject;
        OPENSSL_free(subject);
    }
    return value;
}

/**
 * @brief Read an 'issuer DN' value from currenly loaded X509.
 *
 * @param o_value Read 'issuer DN' value.
 */
void cc::crt::X509Reader::GetIssuerDN (std::string& o_value) const
{
    X509_NAME* name = X509_get_issuer_name(x509_);
    if ( nullptr == name ) {
        char* issuer = X509_NAME_oneline(name_ptr_, NULL, 0);
        if ( nullptr != issuer ) {
            o_value = issuer;
            OPENSSL_free(issuer);
        }
    }
}

/**
 * @brief Read an 'issuer DN' value from currenly loaded X509.
 *
 * @return Read 'issuer DN' value.
 */
std::string cc::crt::X509Reader::GetIssuerDN () const
{
    std::string value;
    X509_NAME* name = X509_get_issuer_name(x509_);
    if ( nullptr == name ) {
        char* issuer = X509_NAME_oneline(name_ptr_, NULL, 0);
        if ( nullptr != issuer ) {
            value = issuer;
            OPENSSL_free(issuer);
        }
    }
    return value;
}

/**
 * @brief Read an entry first value from currenly loaded X509.
 *
 * @param a_nid   Identifier.
 * @param o_value First value found.
 *
 * @return Number of available values.
 */
size_t cc::crt::X509Reader::GetEntry (const int a_nid, std::string& o_value) const
{
    size_t cnt = 0;
    for ( int i = -1 ; ; ) {
        i = X509_NAME_get_index_by_NID(name_ptr_, a_nid, i);
        if ( i < 0 ) {
            break;
        }
        if ( 0 == o_value.size() ) {
            const X509_NAME_ENTRY* entry = X509_NAME_get_entry(name_ptr_, i);
            const ASN1_STRING*     value  = X509_NAME_ENTRY_get_data(entry);
            if ( 0 != ASN1_STRING_length(value) ) {
                ANS1_UTF8_STRING(value, o_value);
            }
        }
        cnt++;
    }
    return cnt;
}

/**
 * @brief Read an entry values from currenly loaded X509.
 *
 * @param a_nid    Identifier.
 * @param o_values Collected values.
 */
void cc::crt::X509Reader::GetEntry (const int a_nid, std::vector<std::string>& o_values) const
{
    std::string tmp;
    for ( int i = -1 ; ; ) {
        i = X509_NAME_get_index_by_NID(name_ptr_, a_nid, i);
        if ( i < 0 ) {
            break;
        }
        const X509_NAME_ENTRY* entry = X509_NAME_get_entry(name_ptr_, i);
        const ASN1_STRING*     value  = X509_NAME_ENTRY_get_data(entry);
        if ( 0 != ASN1_STRING_length(value) ) {
            tmp.clear();
            ANS1_UTF8_STRING(value, tmp);
            o_values.push_back(tmp);
        }
    }
}

/**
 * @brief Load validity info from a X509 certificate.
 *
 * @param a_x509       Pointer o X509 data.
 * @param o_valid_from HR valid notBefore.
 * @param o_valid_to   HR valid notAfter.
 * @param o_status     One of 'expired' or 'valid'.
 */
void cc::crt::X509Reader::GetValidity (std::string& o_valid_from, std::string& o_valid_to, std::string& o_status) const
{
    char tmp [16] = {0} ; // YYYYMMDDHHMMSSZ
    
    const ASN1_TIME* not_before = X509_get0_notBefore(x509_);
    struct tm valid_from = { 0 };
    if ( 1 == ASN1_TIME_to_tm(not_before, &valid_from) ) {
        snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]),"%4d%02d%02d%02d%02d%02dZ",
                 valid_from.tm_year + 1900, valid_from.tm_mon + 1, valid_from.tm_mday, valid_from.tm_hour, valid_from.tm_min, valid_from.tm_sec);
        o_valid_from = tmp;
    }
    
    const ASN1_TIME* not_after = X509_get0_notAfter(x509_);
    struct tm valid_to = { 0 };
    if ( 1 == ASN1_TIME_to_tm(not_after, &valid_to) ) {
        snprintf(tmp, sizeof(tmp) / sizeof(tmp[0]),"%4d%02d%02d%02d%02d%02dZ",
                 valid_to.tm_year + 1900, valid_to.tm_mon + 1, valid_to.tm_mday, valid_to.tm_hour, valid_to.tm_min, valid_to.tm_sec);
        o_valid_to = tmp;
    }
    
    const time_t end = timegm(&valid_to);
    const time_t now = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    
    o_status = ( ( end < now ) ? "expired" : "valid" );
}

/**
 * @brief Dump some info about loaded X509 certificate.
 *
 * param a_fp
 */
void cc::crt::X509Reader::Dump (FILE *a_fp)
{
    std::vector<std::string> tmp_values;

    const std::map<int, const char*> map = {
        { NID_serialNumber          , LN_serialNumber            },
        { NID_commonName            , LN_commonName              },
        { NID_organizationName      , LN_organizationName        },
        { NID_organizationIdentifier, LN_organizationIdentifier  } , // ... OID.2.5.4.97 ...
        { NID_organizationalUnitName, LN_organizationalUnitName  },
        { NID_givenName             , LN_givenName               },
        { NID_surname               , LN_surname                 },
    };

    fprintf(a_fp, "--- --- ---\n");
    for ( auto entry : map ) {
        GetEntry(entry.first, tmp_values);
        fprintf(a_fp, "%s:\n", entry.second);
        for ( auto it : tmp_values ) {
            fprintf(a_fp, "\t: %s\n", it.c_str());
        }
        fflush(a_fp);
        tmp_values.clear();
    }
}

// MARK: PRIVATE

/**
 * @brief Convert an ASN1 UTF8 string to a std::string.
 *
 * @param a_value ASN1 UTF8 string.
 * @param o_value Converted value.
 */
void cc::crt::X509Reader::ANS1_UTF8_STRING (const ASN1_STRING* a_value, std::string& o_value) const
{
    unsigned char * utf8_value = nullptr;
    try {
        int utf8_length = ASN1_STRING_to_UTF8(&utf8_value, a_value);
        if ( utf8_length < 0 ) {
            CC_WARNING_TODO("cc::crt::X509Reader - error handling");
            throw ::cc::Exception("%s", "TODO");
        } else {
            // ... cleanup string - *trailing* NUL byte ...
            while ( utf8_length > 0 && utf8_value[utf8_length - 1] == '\0' ) {
                --utf8_length;
            }
            // ... cleanup string - *embedded* NULs ...
            if ( strlen(reinterpret_cast<const char* const>(utf8_value)) != static_cast<size_t>(utf8_length) ) {
                CC_WARNING_TODO("cc::crt::X509Reader - error handling");
                throw ::cc::Exception("%s", "TODO");
            }
            o_value = std::string(reinterpret_cast<const char* const>(utf8_value));
        }
    } catch (...) {
        if ( nullptr != utf8_value ) {
            OPENSSL_free(utf8_value);
        }
    }
    if ( nullptr != utf8_value ) {
        OPENSSL_free(utf8_value);
    }
}

// MARK: STATIC Method(s) / Function(s)

/**
 * @brief Fold a PEM in to 64 columns.
 *
 * @param a_pem Unfolded PEM data.
 * @return Folded PEM data.
 */
std::string cc::crt::X509Reader::Fold (const char* const a_pem)
{
    std::stringstream ss("");
    const char* ptr = a_pem;
    ss << "-----BEGIN CERTIFICATE-----\n";
    while ( 0 != ptr[0] ) {
        size_t column = 0;
        const char* const str = ptr;
        while ( 0 != ptr[0] && column < 64 ) {
            ptr++;
            column++;
        }
        ss.write(str, ptr - str);
        ss << '\n';
    }
    ss << "-----END CERTIFICATE-----";
    return ss.str();
}
