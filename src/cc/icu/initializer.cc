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
    instance_.icu_data_        = nullptr;
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
#if 1
#else
    struct stat sb;
    void*       data = nullptr;
#endif

    // ... if already loaded ...
    if ( true == initialized_ ) {
        // ... we're done ...
        return last_error_code_;
    }

    // ... clear last error ...
    last_error_code_ = U_FILE_ACCESS_ERROR;
    
#if 1
    FILE* fp = fopen(a_dtl_uri.c_str(), "rb");
    if ( nullptr == fp ) {
        load_error_msg_ = a_dtl_uri + " ~ fopen failed with error " + std::to_string(errno) + " - " + strerror(errno);
        // ... we're done ...
        return last_error_code_;
    }
    // ... calculate required buffer size ...
    fseek(fp, 0, SEEK_END);
    const size_t size = ftell(fp);
    rewind(fp);
    
    // ... allocate buffer data ...
   icu_data_ = new char[size];
   if ( size != fread(icu_data_, sizeof(char), size, fp) ) {
       const int ferrno = ferror(fp);
       load_error_msg_ = a_dtl_uri + " ~ fread failed with error " + std::to_string(ferrno) + " - " + strerror(ferrno);
   }

#else
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
#endif

    // ... reset error code ...
    last_error_code_ = UErrorCode::U_ZERO_ERROR;
    
    // ... stop trying to load ICU data from files ...
    udata_setFileAccess(UDATA_NO_FILES, &last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        load_error_msg_ = a_dtl_uri + " ~ udata_setFileAccess failed with error " + std::to_string(last_error_code_) + " - " + u_errorName(last_error_code_);
        goto leave;
    }
    
    // ... set loaded data ...
#if 1
    udata_setCommonData(icu_data_, &last_error_code_);
#else
    udata_setCommonData(data, &last_error_code_);
#endif
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        load_error_msg_ = a_dtl_uri + " ~ udata_setCommonData failed with error " + std::to_string(last_error_code_) + " - " + u_errorName(last_error_code_);
        goto leave;
    }

    // ... initialize
    u_init(&last_error_code_);
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        load_error_msg_ = a_dtl_uri + " ~ u_init failed with error " + std::to_string(last_error_code_) + " - " + u_errorName(last_error_code_);
        goto leave;
    }
    
leave:
#if 1
    // ... if an error is set ...
    if ( UErrorCode::U_ZERO_ERROR != last_error_code_ ) {
        // ... release previously allocated memory ( if any ) ...
        if ( nullptr != icu_data_ ) {
            delete[] icu_data_;
            icu_data_ = nullptr;
        }
    }
#else
    // ... close file ...
    if ( -1 != fd ) {
        close(fd);
    }
#endif
    // ... success?
    initialized_ = ( UErrorCode::U_ZERO_ERROR == last_error_code_ );
    // .. we're done ...
    return last_error_code_;
}
