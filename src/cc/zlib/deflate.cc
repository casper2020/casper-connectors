/**
 * @file deflate.cc
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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

#include "cc/zlib/deflate.h"

#include "cc/exception.h"

#include <sys/time.h>

#define CC_ZLIB_DEFLATE_RAW -MAX_WBITS
#define CC_ZLIB_DEFLATE_GZIP 31

/**
 * @brief Default constructor.
 */
cc::zlib::Deflate::Deflate ()
{
    chunk_  = nullptr;
    stream_ = nullptr;
    error_ = -1;
}

/**
 * @brief Destructor.
 */
cc::zlib::Deflate::~Deflate ()
{
    if ( nullptr != chunk_ ) {
        delete [] chunk_;
    }
    if ( nullptr != stream_ ) {
        delete stream_;
    }
}

/**
 * @brief Deflate data.
 *
 * @param a_data     Data to compress.
 * @param a_size     Data size.
 * @param a_callback Function to call to delived compressed chunk.
 * @param a_level    Compression level, -1 or 0..9.
 * @param a_gzip     If true GZip format will be used.
 */
void cc::zlib::Deflate::Do (const unsigned char* a_data, const size_t a_size,
                            const std::function<void(const unsigned char*, const size_t)>& a_callback,
                            const int8_t a_level, const bool a_gzip)
{
    // ... sanity check ...
    if ( not ( Z_DEFAULT_COMPRESSION == a_level || ( a_level >= Z_BEST_SPEED && a_level <= Z_BEST_COMPRESSION) ) ) {
        throw ::cc::Exception("An error ocurred while deflating data - invalid compression level of %d!", a_level);
    }
    
    if ( nullptr != chunk_ ) {
        delete [] chunk_;
    }
    chunk_  = new unsigned char[16384];
    if ( nullptr != stream_ ) {
        delete stream_;
    }
    stream_ = new z_stream;
    stream_->zalloc = Z_NULL;
    stream_->zfree  = Z_NULL;
    stream_->opaque = Z_NULL;
    //
    // memLevel - memLevel=1 uses minimum memory but is slow and reduces compression ratio; memLevel=9 uses maximum memory for optimal speed. The default value is 8.
    //
    const int i = deflateInit2(stream_, /* level */ int(a_level), /* method */ Z_DEFLATED, /* windowBits */ a_gzip ? CC_ZLIB_DEFLATE_GZIP : CC_ZLIB_DEFLATE_RAW, /* memLevel */ MAX_MEM_LEVEL, /* strategy */  Z_DEFAULT_STRATEGY);
    switch (i) {
            // Z_STREAM_ERROR, Z_BUF_ERROR
        case Z_STREAM_ERROR:
            throw ::cc::Exception("An error ocurred during deflate initialization - %s!", "Z_STREAM_ERROR!");
        case Z_BUF_ERROR:
            throw ::cc::Exception("An error ocurred during deflate initialization - %s!", "Z_BUF_ERROR!");
        case Z_OK:
            break;
        default:
            throw ::cc::Exception("An error ocurred during deflate initialization - %d!", i);
    }
    stream_->next_in  = (unsigned char*)a_data;
    stream_->avail_in = (unsigned int)a_size;
    do {
        stream_->avail_out = 16384;
        stream_->next_out  = chunk_;
        const int r = deflate(stream_, Z_FINISH);
        switch (r) {
                // Z_STREAM_ERROR, Z_BUF_ERROR
            case Z_STREAM_ERROR:
                throw ::cc::Exception("An error ocurred during a deflate operation - %s!", "Z_STREAM_ERROR!");
            case Z_BUF_ERROR:
                throw ::cc::Exception("An error ocurred during a deflate operation - %s!", "Z_BUF_ERROR!");
            case Z_OK:
            case Z_STREAM_END:
                break;
            default:
                throw ::cc::Exception("An error ocurred during a deflate operation - %d!", r);
        }
        a_callback(chunk_, 16384 - stream_->avail_out);
    }
    while (stream_->avail_out == 0);
    deflateEnd(stream_);
    // ... clean up ...
    delete stream_;
    stream_ = nullptr;
    delete [] chunk_;
    chunk_ = nullptr;
}
