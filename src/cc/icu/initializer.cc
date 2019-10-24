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

#include "cc/icu/initializer.h"

#include <unicode/utypes.h> // u_init
#include <unicode/uclean.h> // u_cleanup
#include <unicode/udata.h>  // udata_setCommonData, udata_setFileAccess

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::icu::OneShot::OneShot (cc::icu::Initializer& a_instance)
    : ::cc::Initializer<::cc::icu::Initializer>(a_instance)
{
    instance_.initialized_     = false;
    instance_.last_error_code_ = UErrorCode::U_ZERO_ERROR;
    instance_.icu_data_        = nullptr;
}

/**
 * @brief Destructor.
 */
cc::icu::OneShot::~OneShot ()
{
    if ( true == instance_.initialized_ ) {
        u_cleanup();
    }
    if ( nullptr != instance_.icu_data_ ) {
        delete [] instance_.icu_data_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Load ICU data.
 *
 * @param a_dlt_uri Data file local URI.
 *
 * @return \link UErrorCode \link, UErrorCode::U_ZERO_ERROR if no error occurred.
 */
const UErrorCode& cc::icu::Initializer::Load (const std::string& a_dtl_uri)
{
    // ... if already loaded ...
    if ( true == initialized_ ) {
        // ... we're done ...
        return last_error_code_;
    }

    // ... clear last error ...
    last_error_code_ = U_FILE_ACCESS_ERROR;
    
    // ... open file ...
    FILE* fp = fopen(a_dtl_uri.c_str(), "rb");
    if ( nullptr == fp ) {
        // ... can't be open, we're done ...
        return last_error_code_;
    }
    
    // ... calculate required buffer size ...
    fseek(fp, 0, SEEK_END);
    const size_t size = ftell(fp);
    rewind(fp);
    
    // ... allocate buffer data ...
    icu_data_ = new char[size];
    if ( size != fread(icu_data_, sizeof(char), size, fp) ) {
        fclose(fp);
        goto leave;
    }
    fclose(fp);

    // ... reset error code ...
    last_error_code_ = UErrorCode::U_ZERO_ERROR;

    // ... set loaded data ...
    udata_setCommonData(reinterpret_cast<void*>(icu_data_), &last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        goto leave;
    }
    
    // ... stop trying to load ICU data from files ...
    udata_setFileAccess(UDATA_ONLY_PACKAGES, &last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        goto leave;
    }
    
    // ... initialize
    u_init(&last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        goto leave;
    }
    
leave:
    // ... if an error is set ...
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        // ... release previously allocated memory ( if any ) ...
        if ( nullptr != icu_data_ ) {
            delete[] icu_data_;
            icu_data_ = nullptr;
        }
    }
    //
    initialized_ = ( nullptr != icu_data_ );
    // .. we're done ...
    return last_error_code_;
}
