/**
 * @file file.cc
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
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

#include "cc/fs/posix/file.h"

#include "cc/fs/posix/dir.h"

#include "cc/fs/exception.h"

#include <unistd.h> // mkstemps
#include <libgen.h> // basename

#ifdef __APPLE__
#pragma mark - File
#endif

/**
 * @brief Default constructor.
 */
cc::fs::posix::File::File ()
{
    mode_ = cc::fs::posix::File::Mode::NotSet;
    fp_   = nullptr;
}

/**
 * @brief Destructor.
 */
cc::fs::posix::File::~File ()
{
    if ( nullptr != fp_ ) {
        fclose(fp_);
    }
}

/**
 * @brief Open a file in write mode.
 *
 * @param a_uri
 * @param a_mode
 */
void cc::fs::posix::File::Open (const std::string& a_uri, const cc::fs::posix::File::Mode& a_mode)
{
    if ( nullptr != fp_ ) {
        throw cc::fs::Exception("Unable to open file '%s' - a file is already open!", a_uri.c_str());
    }
    switch (a_mode) {
        case cc::fs::posix::File::Mode::Read:
            fp_ = fopen(a_uri.c_str(), "r");
            break;
        case cc::fs::posix::File::Mode::Write:
            fp_ = fopen(a_uri.c_str(), "w");
            break;
        default:
            throw cc::fs::Exception("Unable to open file '%s' - mode %hhu not supported!", a_uri.c_str(), a_mode);
    }
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to open file '%s' - %s!", a_uri.c_str(), strerror(errno));
    }
    // ... done ...
    mode_ = a_mode;
    uri_  = a_uri;
}

/**
 * @brief Reda data the currently open file.
 *
 * @param o_data Buffer where that must be written to.
 * @param a_size Number of bytes to write.
 * @param o_eof True if end-of-file reached.
 *
 * @return Number of bytes read.
 */
size_t cc::fs::posix::File::Read (unsigned char* o_data, const size_t a_size, bool& o_eof)
{
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to read data from file - not open!");
    }

    if ( cc::fs::posix::File::Mode::Read != mode_ ) {
        throw cc::fs::Exception("Unable to read data from file '%s' - mode %hhu not supported!", uri_.c_str(), mode_);
    }

    const size_t bytes_read = fread(o_data, sizeof(unsigned char), a_size, fp_);
    if ( ferror(fp_) ) {
        throw cc::fs::Exception("Unable to read data from file '%s' - %s!", uri_.c_str(), strerror(errno));
    }
    o_eof = ( 0 != feof(fp_) );

    return bytes_read;
}

/**
 * @brief Open an unique file ( within a directory ) in write mode.
 *
 * @param a_path      Full path where file should be created.
 * @param a_prefix    File name predix.
 * @param a_extension File extension.
 * @param a_size      Required size in bytes ( just for testing proproces, will not reserve it! )
 */
void cc::fs::posix::File::Open (const std::string& a_path, const std::string& a_prefix, const std::string& a_extension, const size_t& a_size)
{
    if ( nullptr != fp_ ) {
        throw cc::fs::Exception("Unable to create unique file at '%s' - a file is already open!", ( a_path + a_prefix ).c_str());
    }

    // ... if directory does not exist ...
    if ( false == cc::fs::posix::Dir::Exists(a_path.c_str()) ) {
        // ... create it ...
        cc::fs::posix::Dir::Make(a_path.c_str());
    }
    
    // ... ensure there is enough space ...
    cc::fs::posix::Dir::EnsureEnoughFreeSpace(a_path.c_str(), a_size,
        ( "Unable to create unique file at '" + ( a_path + a_prefix ) + "'" ).c_str()
    );

    // ... file will be placed there ...
    uri_ = a_path;
    
    // ... append a filename prefix ( if provided ) ...
    if ( a_prefix.length() > 0 ) {
        uri_ += a_prefix + ".XXXXXX";
    } else {
        uri_ += "XXXXXX";
    }
    
    // ... append a fileextension ( if provided ) ...
    if ( a_extension.length() > 0 ) {
        uri_ += "." + a_extension;
    }
    
    // ... ensure unique file in destination directory ...
    char f_template[PATH_MAX] = { 0, 0 };
    memcpy(f_template, uri_.c_str(), uri_.length());
    int fd = mkstemps(f_template, sizeof(char) * static_cast<int>(a_extension.length() + 1));
    if ( -1 == fd ) {
        throw cc::fs::Exception("Unable to create unique file at '%s' - %s!", (a_path + a_prefix).c_str(), strerror(errno));
    }
    
    // ... and open file in 'write' mode ...
    fp_  = fdopen(fd, "w");
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to open unique file '%s' - %s!", uri_.c_str(), strerror(errno));
    }

    // ... done ...
    mode_ = cc::fs::posix::File::Mode::Write;
    uri_  = f_template;
}

/**
 * @brief Writer data the currently open file.
 *
 * @param a_data Data to write.
 * @param a_size Number of bytes to write.
 * @param a_flush When true, data will be flushed immediately.
 *
 * @return Number of bytes written.
 */
size_t cc::fs::posix::File::Write (const unsigned char* a_data, const size_t a_size, const bool a_flush)
{
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to write data to file - not open!");
    }
    
    if ( cc::fs::posix::File::Mode::Write != mode_ ) {
        throw cc::fs::Exception("Unable to write data to file '%s' - mode %hhu not supported!", uri_.c_str(), mode_);
    }
    
    const size_t bytes_written = fwrite(a_data, sizeof(unsigned char), a_size, fp_);
    if ( a_size != bytes_written ) {
        throw cc::fs::Exception("Unable to write data to file '%s' - bytes written differs!", uri_.c_str());
    } else if ( ferror(fp_) ) {
        throw cc::fs::Exception("Unable to write data to file '%s' - %s!", uri_.c_str(), strerror(errno));
    }
    
    if ( true == a_flush ) {
        if ( 0 != fflush(fp_) ) {
            throw cc::fs::Exception("Unable to flush data to file '%s' - %s!", uri_.c_str(), strerror(errno));
        }
    }
    
    return bytes_written;
}

/**
 * @brief Flush data to currently open file.
 */
void cc::fs::posix::File::Flush ()
{
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to flush data to file - not open!");
    }
    if ( cc::fs::posix::File::Mode::Write != mode_ ) {
        throw cc::fs::Exception("Unable to flush data to file '%s' - mode %hhu not supported!", uri_.c_str(), mode_);
    }
    if ( 0 != fflush(fp_) ) {
        throw cc::fs::Exception("Unable to flush data to file '%s' - %s!", uri_.c_str(), strerror(errno));
    }
}

/**
 * @brief Close the currently open file.
 *
 * @param a_force When true all errors will be ignored.
 */
void cc::fs::posix::File::Close (const bool a_force)
{
    if ( nullptr == fp_ ) {
        return;
    }
    if ( 0 != fclose(fp_) ) {
        fp_ = nullptr;
        if ( a_force == false ) {
            uri_ = "";
            throw cc::fs::Exception("Unable to close file '%s' - %s!", uri_.c_str(), strerror(errno));
        }
    } else {
        fp_ = nullptr;
    }
    uri_ = "";
}

/**
 * @brief Obtain currenlty open file size.
 */
uint64_t cc::fs::posix::File::Size ()
{
    return cc::fs::posix::File::Size(uri_.c_str());
}

#ifdef __APPLE__
#pragma mark - STATIC METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Extract a filename from an URI.
 *
 * @param a_uri File URI.
 */
void cc::fs::posix::File::Name (const std::string& a_uri, std::string& o_name)
{
    const size_t l1 = a_uri.length();
    if ( l1 >= PATH_MAX ) {
        throw cc::fs::Exception("Unable to extract file name from path '%s': max path overflow!", a_uri.c_str());
    }
    
    char path[PATH_MAX];
    
    memcpy(path, a_uri.c_str(), l1);
    path[l1] = 0;
    
    const char* const ptr = basename(path);

    const size_t l2 = strlen(ptr);
    if ( 0 == l2 || ( 1 == l2 && '/' == ptr[l2-1] ) || ( 1 == l2 && '.' == ptr[l2-1] ) || ( 2 == l2 && '.' == ptr[l2-1] && '.' == ptr[l2-2] ) ) {
        throw cc::fs::Exception("Unable to extract file name from path '%s': is not a valid file name!", ptr);
    }
    
    o_name = ptr;
}

/**
 * @brief Check if a file exits.
 *
 * @param a_uri File URI.
 *
 * @return True if exists, false otherwise.
 */
bool cc::fs::posix::File::Exists (const std::string& a_uri)
{
    struct stat stat_info;
    if ( 0 == stat(a_uri.c_str(), &stat_info) ) {
        return ( 0 != S_ISREG(stat_info.st_mode) );
    }
    const int ec = errno;
    if ( ENOENT == ec ) {
        return false;
    }
    throw cc::fs::Exception("Unable to check if file '%s' exists - %s!", a_uri.c_str(), strerror(errno));
}

/**
 * @brief Erase a file
 *
 * @param a_uri File URI.
 */
void cc::fs::posix::File::Erase (const std::string& a_uri)
{
    const int rv = unlink(a_uri.c_str());
    if ( 0 != rv ) {
        throw cc::fs::Exception("Unable to erase file '%s' - %s!", a_uri.c_str(), strerror(errno));
    }
}

/**
 * @brief Erase a file
 *
 * @param a_from_uri Old file URI.
 * @param a_to_uri New file URI.
 */
void cc::fs::posix::File::Rename (const std::string& a_from_uri, const std::string& a_to_uri)
{
    const int rv = rename(a_from_uri.c_str(), a_to_uri.c_str());
    if ( 0 != rv ) {
        throw cc::fs::Exception("Unable to rename file '%s' to '%s' - %s!", a_from_uri.c_str(), a_to_uri.c_str(), strerror(errno));
    }
}

/**
 * @brief Obtain a file size.
 *
 * @param a_uri File URI.
 */
uint64_t cc::fs::posix::File::Size (const std::string& a_uri)
{
    struct stat stat_info;
    if ( stat(a_uri.c_str(), &stat_info) == 0 ) {
        if ( S_ISREG(stat_info.st_mode) != 0 ) {
            return stat_info.st_size;
        } else {
            throw cc::fs::Exception("Unable to obtain the file '%s' size - it does not exist!", a_uri.c_str());
        }
    } else {
        throw cc::fs::Exception("Unable to obtain the file '%s' size - %s!", a_uri.c_str(), strerror(errno));
    }
}

/**
 * @brief Ensure a unique file within a directory.
 *
 * @param a_path      Full path where file should be created.
 * @param a_name      File name.
 * @param a_extension File extension.
 *
 * @param o_uri Full URI to new unique file.
 */
void cc::fs::posix::File::Unique (const std::string& a_path, const std::string& a_name, const std::string& a_extension,
                                  std::string& o_uri)
{
    if ( a_name.length() > 0 ) {
        o_uri = a_path + a_name + ".XXXXXX." + a_extension;
    } else {
        o_uri = a_path + "XXXXXX." + a_extension;
    }
    
    char f_template[PATH_MAX] = { 0, 0 };
    memcpy(f_template, o_uri.c_str(), o_uri.length());
    int fd = mkstemps(f_template, sizeof(char) * static_cast<int>(a_extension.length() + 1));
    if ( -1 == fd ) {
        throw cc::Exception("%d - %s", errno, strerror(errno));
    } else {
        close(fd);
    }
    
    o_uri = f_template;
}
