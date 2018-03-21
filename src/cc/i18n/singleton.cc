/**
 * @file i18.cc
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

#include "cc/i18n/singleton.h"

#include "osal/osal_file.h"

#include <iostream> // std::istream, std::ios
#include <fstream>  // std::filebuf

//
// STATIC CONST DATA
//
const std::map<uint16_t, std::string> cc::i18n::Singleton::k_http_status_codes_map_ = {
    /* 4xx */
    { 400, "Bad Request"                     },
    { 401, "Unauthorized"                    },
    { 402, "Payment Required"                },
    { 403, "Forbidden"                       },
    { 404, "Not Found"                       },
    { 405, "Method Not Allowed"              },
    { 406, "Not Acceptable"                  },
    { 407, "Proxy Authentication Required"   },
    { 408, "Request Timeout"                 },
    { 409, "Conflict"                        },
    { 410, "Gone"                            },
    { 411, "Length Required"                 },
    { 412, "Precondition Failed"             },
    { 413, "Payload Too Large"               },
    { 414, "URI Too Long"                    },
    { 415, "Unsupported Media Type"          },
    { 416, "Requested Range Not Satisfiable" },
    { 417, "Expectation Failed"              },
    { 421, "Misdirected Request"             },
    { 426, "Upgrade Required"                },
    { 428, "Precondition Required"           },
    { 429, "Too Many Requests"               },
    { 431, "Request Header Fields Too Large" },
    { 451, "Unavailable For Legal Reasons"   },
    /* 5xx */
    { 500, "Internal Server Error"           },
    { 501, "Not Implemented"                 },
    { 502, "Bad Gateway"                     },
    { 503, "Service Unavailable"             },
    { 504, "Gateway Timeout"                 },
    { 505, "HTTP Version Not Supported"      },
    { 506, "Variant Also Negotiates"         },
    { 507, "Variant Also Negotiates"         },
    { 511, "Network Authentication Required" }
};

/**
 * @brief One-shot initializer.
 *
 * @param a_resources_dir
 * @param a_failure_callback
 */
void cc::i18n::Singleton::Startup (const std::string& a_resources_dir,
                                   const std::function<void(const char* const a_i18_key, const std::string& a_file, const std::string& a_reason)>& a_failure_callback)
{
    localization_ = Json::Value::null;

    std::string i18_file = a_resources_dir;
    if ( '/' != i18_file[i18_file.length() - 1] ) {
        i18_file += "/i18.json";
    } else {
        i18_file += "i18.json";
    }
    
    if ( false == osal::File::Exists(i18_file.c_str()) ) {
        a_failure_callback("BROKER_MISSING_OR_INVALID_RESOURCE_FILE",
                           i18_file,
                           "File does not exist!"
        );
        return;
    }
    
    std::filebuf file;
    if ( nullptr == file.open(i18_file, std::ios::in) ) {
        a_failure_callback("BROKER_MISSING_OR_INVALID_RESOURCE_FILE",
                           i18_file,
                           "Unable to open file!"
        );
        return;
    }
    
    std::istream in_stream(&file);
    Json::Reader reader;
    
    if ( false == reader.parse(in_stream, localization_, false)  ) {
        localization_ = Json::Value::null;
        a_failure_callback("BROKER_MISSING_OR_INVALID_RESOURCE_FILE",
                           i18_file,
                           "Unable to parse file!"
        );
    } else if ( true == localization_.isNull() ) {
        a_failure_callback("BROKER_MISSING_OR_INVALID_RESOURCE_FILE",
                           i18_file,
                           "Nothing to load!"
        );
    }
    
    file.close();
}

/**
 * @brief Dealloc previously allocated memory ( if any ).
 */
void cc::i18n::Singleton::Shutdown ()
{
    localization_ = Json::Value::null;
}

