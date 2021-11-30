/**
 * @file value.cc
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ev/curl/value.h"

/**
 * @brief Default constructor.
 *
 * @param a_code
 * @param a_headers
 * @param a_body
 * @param a_rtt
 */
ev::curl::Value::Value (const int a_code, const EV_CURL_HEADERS_MAP& a_headers, const std::string& a_body,
                        const size_t& a_rtt)
: curl::Object(ev::Object::Type::Value),
    code_(a_code), rtt_(a_rtt),
    http_version_(0.0f),
    body_(a_body), last_modified_(0)
{
    for ( auto m : a_headers ) {
        for ( auto p : m.second ) {
            headers_[m.first].push_back(p);
        }
    }
}

/**
 * @brief Copy constructor.
 *
 * @param a_value Object to copy.
 */
ev::curl::Value::Value (const ev::curl::Value& a_value)
: curl::Object(a_value.type_),
    code_(a_value.code_), rtt_(a_value.rtt_),
    http_version_(a_value.http_version_), url_(a_value.url_),
    body_(a_value.body_), last_modified_(a_value.last_modified_)
{
    for ( auto m : a_value.headers_ ) {
        for ( auto p : m.second ) {
            headers_[m.first].push_back(p);
        }
    }
}

/**
 * @brief Destructor.
 */
ev::curl::Value::~Value ()
{
    /* empty */
}

// MARK: - [STATIC] Public - Method(s) / Function(s)

/**
 * @brief Extract 'Content-Disposition' header components.
 *
 * @param disposition  The original name of the file transmitted ( optional ).
 * @param field_name   The name of the HTML field in the form that the content of this subpart refer to.
 *                       A name with a value of '_charset_' indicates that the part is not an HTML field,
 *                       but the default charset to use for parts without explicit charset information.
 * @param file_name    Is followed by a string containing the original name of the file transmitted.
 *                     The filename is always optional and must not be used blindly by the application:
 *                     path information should be stripped, and conversion to the server file system rules should be done.
 *
 * @return NGX_OK or NGX_ERROR.
 */
bool ev::curl::Value::content_disposition (std::string* o_disposition, std::string* o_field_name, std::string* o_file_name) const
{
    // ... nothing to do?
    if ( nullptr == o_disposition && nullptr == o_field_name && nullptr == o_file_name ) {
        return false;
    }
    // ... no header?
    const std::string h = header("Content-Disposition");
    if ( 0 == h.length() ) {
        return false;
    }
    // ... invalid header value?
    const char* disposition_ptr = strcasestr(h.c_str(), "Content-Disposition:");
    if ( nullptr == disposition_ptr ) {
        return false;
    }
    // ... skip while spaces and separators ...
    disposition_ptr += strlen("Content-Disposition:");
    while ( ' ' == disposition_ptr[0] && not ( '\0' == disposition_ptr[0] || ';' == disposition_ptr[0] ) ) {
        disposition_ptr++;
    }
    // ... nothing else?
    if ( '\0' == disposition_ptr[0] ) {
        return false;
    }
    // ... extract data ...
    std::string disposition,  field_name, file_name;
    const char* sep = strchr(disposition_ptr, ';');
    if ( nullptr != sep ) {
        disposition = std::string(disposition_ptr, sep - disposition_ptr);
        disposition_ptr += ( sep - disposition_ptr );
        // ... has name ?
        typedef struct {
            const char* const name_;
            std::string&      value_;
        } Entry;
        
        const Entry entries[2] = {
            { "name"    , field_name },
            { "filename", file_name  }
        };
        for ( auto entry : entries ) {
            sep = strchr(disposition_ptr, ';');
            if ( nullptr != sep ) {
                sep++;
                while ( ' ' == sep[0] && not ( '\0' == sep[0] ) ) {
                    sep++;
                }
                const std::string search_prefix = std::string(entry.name_) + "=\"";
                if ( 0 == strncasecmp(search_prefix.c_str(), sep, search_prefix.length()) ) {
                    disposition_ptr = sep + search_prefix.length();
                    sep = strchr(disposition_ptr, '"');
                    if ( nullptr != sep ) {
                        entry.value_ = std::string(disposition_ptr, sep - disposition_ptr);
                        sep++;
                    } else {
                        return false;
                    }
                }
            }
        }
    } else {
        disposition = disposition_ptr;
    }
    // ... _charset_ exception ...
    if ( 0 != strncasecmp(field_name.c_str(), "_charset_", strlen("_charset_")) ) {
        field_name = "";
    }
    // ... set ouput ...
    if ( nullptr != o_disposition ) {
        (*o_disposition) = disposition;
    }
    if ( nullptr != o_field_name ) {
        (*o_field_name) = field_name;
    }
    if ( nullptr != o_file_name ) {
        (*o_file_name) = file_name;
    }
    // ... done ...
    return true;
}
