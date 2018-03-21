/**
 * @file writer.cc
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

#include "cc/file/writer.h"

#include "cc/file/exception.h"
#include <string.h> // strerror

/**
 * @brief Default constructor.
 */
cc::file::Writer::Writer ()
{
    fp_ = nullptr;
}

/**
 * @brief Destructor.
 */
cc::file::Writer::~Writer ()
{
    Close(/* a_force */ true);
}

/**
 * @brief Open a file in write mode.
 *
 * @param a_uri
 */
void cc::file::Writer::Open (const std::string& a_uri)
{
    Close(/* a_force */ true);
    fp_ = fopen(a_uri.c_str(), "w");
    if ( nullptr == fp_ ) {
        throw cc::file::Exception("Unable to open file '%s' - %s!", uri_.c_str(), strerror(errno));
    }
    uri_ = a_uri;
}

/**
 * @brief Writer data the currently open file.
 *
 * @param a_data
 * @param a_size
 * @param a_flush
 *
 * @return Number of bytes written .
 */
size_t cc::file::Writer::Write (const unsigned char* a_data, const size_t a_size, const bool a_flush)
{
    if ( nullptr == fp_ ) {
        throw cc::file::Exception("Unable to write data to file - not open!");
    }
    
    const size_t bytes_written = fwrite(a_data, sizeof(unsigned char), a_size, fp_);
    if ( a_size != bytes_written ) {
        throw cc::file::Exception("Unable to write data to file '%s' - bytes written differs!", uri_.c_str());
    } else if ( ferror(fp_) ) {
        throw cc::file::Exception("Unable to write data to file '%s' - %s!", uri_.c_str(), strerror(errno));
    }
    
    if ( true == a_flush ) {
        if ( 0 != fflush(fp_) ) {
            throw cc::file::Exception("Unable to flush data to file '%s' - %s!", uri_.c_str(), strerror(errno));
        }
    }
    
    return bytes_written;
}

/**
 * @brief Close the currently open file.
 *
 * @param a_force When true all errors will be ignored.
 */
void cc::file::Writer::Close (const bool a_force)
{
    if ( nullptr == fp_ ) {
        return;
    }
    if ( 0 != fclose(fp_) ) {
        fp_ = nullptr;
        if ( a_force == false ) {
            uri_ = "";
            throw cc::file::Exception("Unable to close file '%s' - %s!", uri_.c_str(), strerror(errno));
        }
    } else {
        fp_ = nullptr;
    }
    uri_ = "";
}
