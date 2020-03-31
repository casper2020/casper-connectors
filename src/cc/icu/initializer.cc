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

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>  // open
#include <unistd.h> // close
#include <errno.h>  // errno
#include <string.h> // strlen, strerror, ...

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
}

/**
 * @brief Destructor.
 */
cc::icu::OneShot::~OneShot ()
{
    if ( true == instance_.initialized_ ) {
        u_cleanup();
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
    struct stat sb;
    void*       data = nullptr;

    // ... if already loaded ...
    if ( true == initialized_ ) {
        // ... we're done ...
        return last_error_code_;
    }

    // ... clear last error ...
    last_error_code_ = U_FILE_ACCESS_ERROR;
    
    // ... open file ...
    int fd = open(a_dtl_uri.c_str(), O_RDONLY);
    if ( -1 == fd ) {
        load_error_msg_ = a_dtl_uri + " ~ fopen failed with error " + std::to_string(errno) + " - " + strerror(errno);
        goto leave;
    }
    
    // .. gather file info ...
    if ( -1 == fstat(fd, &sb) ) {
        load_error_msg_ = a_dtl_uri + " ~ fstat failed with error " + std::to_string(errno) + " - " + strerror(errno);
        goto leave;
    }
    
    // ... map it ...
    data = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if ( MAP_FAILED == data ) {
        load_error_msg_ = a_dtl_uri + " ~ mmap failed with error " + std::to_string(errno) + " - " + strerror(errno);
        goto leave;
    }
    
    //  ..tell the kernel that accesses are likely to be random rather than sequential ..
    if ( -1 == madvise(data, sb.st_size, MADV_RANDOM) ) {
        load_error_msg_ = a_dtl_uri + " ~ madvise failed with error " + std::to_string(errno) + " - " + strerror(errno);
        goto leave;
    }

    // ... reset error code ...
    last_error_code_ = UErrorCode::U_ZERO_ERROR;
    
    // ... stop trying to load ICU data from files ...
    udata_setFileAccess(UDATA_ONLY_PACKAGES, &last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        goto leave;
    }
    
    // ... set loaded data ...
    udata_setCommonData(data, &last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        goto leave;
    }

    // ... initialize
    u_init(&last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        goto leave;
    }
    
leave:
    // ... close file ...
    if ( -1 != fd ) {
        close(fd);
    }
    // ... success?
    initialized_ = ( UErrorCode::U_ZERO_ERROR == last_error_code_ );
    // .. we're done ...
    return last_error_code_;
}
