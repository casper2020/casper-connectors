/**
 * @file mime_type.cc
 *
 * Copyright (c) 2017-2023 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/magic/mime_type.h"

#include "cc/exception.h"
#include "cc/fs/dir.h"
#include "cc/fs/file.h"

#include "cc/types.h"
#include "cc/debug/types.h"

const std::string cc::magic::MIMEType::sk_pdf_ = { 0x25, 0x50, 0x44, 0x46, 0x2d }; // %PDF-

/**
 * @brief Default constructor.
 */
cc::magic::MIMEType::MIMEType ()
{
    cookie_ = nullptr;
    buffer_[0] = 0;
}

/**
 * @brief Destructor
 */
cc::magic::MIMEType::~MIMEType ()
{
    if ( nullptr != cookie_ ) {
        magic_close(cookie_);
    }
}

/**
 * @brief Initializer magic library.
 *
 * @param a_shared_directory Shared directory URI.
 * @param a_flags            Default MAGIC_MIME_TYPE.
 */
void cc::magic::MIMEType::Initialize (const std::string& a_shared_directory, int a_flags)
{
    if ( nullptr != cookie_ ) {
        return;
    }
    
    cookie_ = magic_open(a_flags);
    if ( nullptr == cookie_ ) {
        throw cc::Exception("Unable to initialize magic library!");
    }
        
#ifdef __APPLE__
    const std::string mgc_file_uri = ::cc::fs::Dir::Normalize(a_shared_directory) + CC_IF_DEBUG_ELSE("libmagic/debug/magic.mgc", "libmagic/magic.mgc");
#else
    const std::string mgc_file_uri = ::cc::fs::Dir::Normalize(a_shared_directory) + "libmagic/magic.mgc";
#endif
    if ( 0 == magic_load(cookie_, mgc_file_uri.c_str() ) ) {
        return;
    }
    
    const std::string error = magic_error(cookie_);
    magic_close(cookie_);
    cookie_ = nullptr;
    throw cc::Exception("Cannot load magic database from: %s - %s!", mgc_file_uri.c_str(), error.c_str());
}

/**
 * @brief Reset magic library flags.
 *
 * @param a_flags Library flags
 */
void cc::magic::MIMEType::Reset (const int a_flags)
{
    if ( nullptr == cookie_ ) {
        throw cc::Exception("Magic library is not initialized!");
    }

    if ( 0 != magic_setflags(cookie_, a_flags) ) {
        throw cc::Exception("Unable to set magic library flags %0x8X!", a_flags);
    }
}

/**
 * @brief Obtain a file's MIME Type.
 *
 * @param a_uri Local file URI.
 *
 * @return File's MIME Type value ( / encode if previously requested ), "" if not found.
 */
std::string cc::magic::MIMEType::MIMETypeOf (const std::string& a_uri) const
{
    if ( nullptr == cookie_ ) {
        throw cc::Exception("Magic library is not initialized!");
    }
    
    if ( false == ::cc::fs::File::Exists(a_uri) ) {
        throw cc::Exception("File %s does not exist!", a_uri.c_str());
    }
    
    const char* const mime = magic_file(cookie_, a_uri.c_str());
    if ( nullptr != mime ) {
        return mime;
    } else {
        return "";
    }
}

/**
 * @brief Obtain a file's MIME Type.
 *
 * @param a_uri Local file URI.
 *
 * @return File's MIME Type value ( / encode if previously requested ), "" if not found.
 *
 * @return File's MIME Type value ( / encode if previously requested ), "" if not found.
 */
std::string cc::magic::MIMEType::MIMETypeOf (const std::string& a_uri, std::size_t& o_offset)
{
    std::string mime     = MIMETypeOf(a_uri);
                o_offset = 0;
    if ( 0 == strcasecmp(mime.c_str(), "application/octet-stream") ) {
        // ... 2nd attempt: f#c%edup PDF ...
        ::cc::fs::File reader;
        try {
            size_t        read = 0;
            bool          eof  = false;
            reader.Open(a_uri, ::cc::fs::File::Mode::Read);
            if ( reader.Size() >= sk_pdf_.size() && ( read = reader.Read(buffer_, sizeof(buffer_) / sizeof(buffer_[0]), eof) ) >= sk_pdf_.size() && false == eof ) {
#if defined(CC_CPP_VERSION) && CC_CPP_VERSION >= 17L
                std::string_view sv(reinterpret_cast<const char*>(buffer_), read);
#else
		std::string sv(reinterpret_cast<const char*>(buffer_), read);
#endif
                if ( ( o_offset = sv.find(sk_pdf_) ) != sv.npos ) {
                    mime = "application/pdf";
                }
            }
            reader.Close();
        } catch (...) {
            // ... eat it ...
            reader.Close();
        }
    }
    // ... done ...
    return mime;
}

/**
 * @brief Obtain a file's MIME Type without charset or it's description ( depends on flags ).
 *
 * @param a_uri Local file URI.
 *
 * @return File's MIME Type or Description value, "" if not found.
 */
std::string cc::magic::MIMEType::WithoutCharsetOf (const std::string& a_uri)
{
    std::string type = MIMETypeOf(a_uri);
    const char* p = strcasestr(type.c_str(), "; charset=");
    if ( nullptr != p ) {
        type = std::string(type.c_str(), size_t(p - type.c_str()));
    }
    return type;
}
