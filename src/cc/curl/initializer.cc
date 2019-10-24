/**
* @file initializer.cc
*
* Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors..
*
* casper-connectors. is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-connectors.  is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-connectors..  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cc/curl/initializer.h"

#include <curl/curl.h> // curl_global_init(...), curl_global_cleanup

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::curl::OneShot::OneShot (cc::curl::Initializer& a_instance)
    : ::cc::Initializer<::cc::curl::Initializer>(a_instance)
{
    instance_.initialized_ = false;
}

/**
 * @brief Destructor.
 */
cc::curl::OneShot::~OneShot ()
{
    if ( true == instance_.initialized_ ) {
        curl_global_cleanup();
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Initialize cURL.
 *
 * @return \link CURLcode \link
 */
CURLcode cc::curl::Initializer::Start ()
{
    if ( false == initialized_ ) {
        const CURLcode rv = curl_global_init(CURL_GLOBAL_ALL);
        if ( CURLE_OK == rv ) {
            initialized_ = true;
        }
        return rv;
    } else {
        return CURLE_OK;
    }
}
