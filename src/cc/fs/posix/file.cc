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

#include "cc/types.h"

#include <unistd.h>  // mkstemps
#include <libgen.h>  // basename, dirname
#include <dirent.h>  // opendir
#include <fnmatch.h> // fnmatch

#include <string.h> // strlen, strerror, ...

// statfs
#ifdef __APPLE__
  #include <sys/param.h>
  #include <sys/mount.h>
  #include <sys/syslimits.h>
#else // linux
  #include <sys/vfs.h>    /* or <sys/statfs.h> */
  #include <linux/limits.h>
#endif

#include "cc/hash/md5.h"

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
        case cc::fs::posix::File::Mode::Append:
            fp_ = fopen(a_uri.c_str(), "a");
            break;
        default:
            throw cc::fs::Exception("Unable to open file '%s' - mode " UINT8_FMT " not supported!",
				    a_uri.c_str(),
				    static_cast<uint8_t>(a_mode)
	    );
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
        throw cc::fs::Exception("Unable to read data from file '%s' - mode " UINT8_FMT " not supported!",
				uri_.c_str(),
				static_cast<uint8_t>(mode_)
	);
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
    if ( false == cc::fs::posix::Dir::Exists(a_path) ) {
        // ... create it ...
        cc::fs::posix::Dir::Make(a_path);
    }
    
    // ... ensure there is enough space ...
    cc::fs::posix::Dir::EnsureEnoughFreeSpace(a_path, a_size,
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
    
    const int suffix_length = ( a_extension.length() > 0 ? static_cast<int>(a_extension.length() + sizeof(char)) : 0 );
    
    // ... ensure unique file in destination directory ...
    char f_template[PATH_MAX] = { 0, 0 };
    memcpy(f_template, uri_.c_str(), uri_.length());
    int fd = mkstemps(f_template, suffix_length);
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
    
    if ( not ( cc::fs::posix::File::Mode::Write == mode_ || cc::fs::posix::File::Mode::Append == mode_ ) ) {
        throw cc::fs::Exception("Unable to write data to file '%s' - mode " UINT8_FMT " not supported!",
				uri_.c_str(),
				static_cast<uint8_t>(mode_)
        );
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
 * @brief Writer data the currently open file.
 *
 * @param a_data
 * @param a_flush When true, data will be flushed immediately.
 *
 * @return Number of bytes written.
 */
size_t cc::fs::posix::File::Write (const std::string& a_data, const bool a_flush)
{
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to write data to file - not open!");
    }
    
    if ( not ( cc::fs::posix::File::Mode::Write == mode_ || cc::fs::posix::File::Mode::Append == mode_ ) ) {
        throw cc::fs::Exception("Unable to write data to file '%s' - mode " UINT8_FMT " not supported!",
                                uri_.c_str(),
                                static_cast<uint8_t>(mode_)
       );
    }
    
    const size_t bytes_written = fwrite(a_data.c_str(), sizeof(char), a_data.length(), fp_);
    if ( a_data.length() != bytes_written ) {
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
 * @brief Go to a specifc byte in file.
 *
 * @param a_pos Position to seek to.
 */
void cc::fs::posix::File::Seek (const size_t& a_pos)
{
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to seek to position - file not open!");
    }
    if ( 0 != fseek(fp_, a_pos, SEEK_SET) ) {
        throw ::cc::Exception("Could not write seek to " SIZET_FMT ": %s !", a_pos, strerror(errno));
    }
}

/**
 * @brief Flush data to currently open file.
 */
void cc::fs::posix::File::Flush ()
{
    if ( nullptr == fp_ ) {
        throw cc::fs::Exception("Unable to flush data to file - not open!");
    }
    if ( not ( cc::fs::posix::File::Mode::Write == mode_ || cc::fs::posix::File::Mode::Append == mode_ ) ) {
        throw cc::fs::Exception("Unable to flush data to file '%s' - mode " UINT8_FMT " not supported!",
				uri_.c_str(),
				static_cast<uint8_t>(mode_)
	);
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
 * @param o_name Extracted file extension.
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
 * @brief Extract a extension from an URI.
 *
 * @param a_uri       File URI.
 * @param o_extension Extracted file extension.
 */
void cc::fs::posix::File::Extension (const std::string& a_uri, std::string& o_extension)
{
    const size_t l1 = a_uri.length();
    if ( l1 >= PATH_MAX ) {
        throw cc::fs::Exception("Unable to extract file name from path '%s': max path overflow!", a_uri.c_str());
    }
    
    char path[PATH_MAX];
    
    memcpy(path, a_uri.c_str(), l1);
    path[l1] = 0;
    
    const char* const ptr = strrchr(basename(path), '.');
    if ( nullptr != ptr ) {
        o_extension = ptr + sizeof(char);
    } else {
        o_extension = "";
    }
}

/**
 * @brief Extract a path from an URI
 *
 * @param a_uri  The URI.
 * @param o_path The path.
 */
void cc::fs::posix::File::Path (const std::string& a_uri, std::string& o_path)
{
    const size_t l1 = a_uri.length();
    if ( l1 >= PATH_MAX ) {
        throw cc::fs::Exception("Unable to obtain path from URI '%s': max path overflow!", a_uri.c_str());
    }
    
    char uri[PATH_MAX];
    
    memcpy(uri, a_uri.c_str(), l1);
    uri[l1] = 0;
    
    const char* const ptr = dirname(uri);
    if ( nullptr == ptr || '\0' == ptr[0] ) {
        throw cc::fs::Exception("Unable to extract path from URI '%s': is not a valid URI!", uri);
    }
    
    o_path = cc::fs::posix::Dir::Normalize(ptr);
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
 * @brief Erase files in a specific directory that match a pattern
 *
 * @param a_dir     Local directory URI.
 * @param a_pattern Pattern to apply to file list.
 */
void cc::fs::posix::File::Erase (const std::string& a_dir, const std::string& a_pattern)
{
    // ... invalid arguments?
    if ( 0 == a_dir.length() || 0 == a_pattern.length() ) {
        throw cc::fs::Exception("Unable to erase file(s) at '%s' - invalid arguments!", a_dir.c_str());
    }
    // ... open directory ...
    DIR* handle = opendir(a_dir.c_str());
    if ( nullptr == handle ) {
        throw cc::fs::Exception("Unable to erase file(s) at '%s' - %s!", a_dir.c_str(), strerror(errno));
    }
    // ... find file in that match pattern ...
    char   path [PATH_MAX] = {0, 0};
    struct dirent* entry;
    while ( (entry = readdir(handle)) ) {
        if ( entry->d_type & DT_REG ) {
            if ( 0 == fnmatch(a_pattern.c_str(), entry->d_name, FNM_CASEFOLD) ) {
                snprintf(path, sizeof(path), "%s%s", a_dir.c_str(), entry->d_name);
                Erase(path);
            }
        }
    }
    // ... done ...
    closedir(handle);
}

/**
 * @brief Move a file.
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
 * @brief Copy a file.
 *
 * @param a_from_uri Old file URI.
 * @param a_to_uri New file URI.
 * @param a_overwrite When true, overwrite destination file ( if any ).
 * @param o_md5 When not null md5 will be calculated.
 */
void cc::fs::posix::File::Copy (const std::string& a_from_uri, const std::string& a_to_uri, const bool a_overwrite, std::string* o_md5)
{
    // ... source file must exist ...
    if ( false == cc::fs::posix::File::Exists(a_from_uri) ) {
        throw cc::fs::Exception("Unable to copy file '%s' to '%s' - %s!",
                                a_from_uri.c_str(), a_to_uri.c_str(), "source file does not exist!"
        );
    }
    
    // ... destination file  ...
    if ( true == cc::fs::posix::File::Exists(a_to_uri) ) {
        // ... exists and ...
        if ( false == a_overwrite ) {
            // ... can't be overriden ...
            throw cc::fs::Exception("Unable to copy file '%s' to '%s' - %s!",
                a_from_uri.c_str(), a_to_uri.c_str(), "destination file already exist!"
            );
        } else {
            // ... can be overriden ...
            cc::fs::posix::File::Erase(a_to_uri);
        }
    }
    
    // ... open source file ...
    cc::fs::posix::File src_file;
    src_file.Open(a_from_uri, cc::fs::posix::File::Mode::Read);
    
    cc::hash::MD5* md5 = nullptr;

    // ... copy source file to destination file ...
    unsigned char* buffer = new unsigned char [4096];
    try {
        
        if ( nullptr != o_md5 ) {
            md5 = new cc::hash::MD5();
            md5->Initialize();
        }
        // ... open destination file ...
        cc::fs::posix::File dst_file;
        dst_file.Open(a_to_uri, cc::fs::posix::File::Mode::Write);
        // ... copy loop ...
        bool   eof        = false;
        size_t bytes_read = 0;
        do {
            // ... read and write ( write function will test bytes written ) ...
            bytes_read = dst_file.Write(buffer, src_file.Read(buffer, 4096, eof), /* a_flush */ true);
            if ( nullptr != md5 ) {
                md5->Update(buffer, bytes_read);
            }
        } while ( eof == false );
        // ... close file ...
        dst_file.Close();
        // ... at least, ensure file has the same size ...
        if ( cc::fs::posix::File::Size(a_from_uri) != cc::fs::posix::File::Size(a_to_uri) ) {
            // ... notify ( file and buffer will be deleted in catch statement )  ...
            throw cc::fs::Exception("Failed to copy file '%s' to '%s' - %s!",
                                    a_from_uri.c_str(), a_to_uri.c_str(), "destination file size mismatch after copy!"
            );
        }
        if ( nullptr != md5 ) {
            (*o_md5) = md5->Finalize();
        }
    } catch (const cc::fs::Exception& a_fs_exception) {
        // ... release previously allocated memory ...
        delete [] buffer;
        if ( nullptr != md5 ) {
            delete md5;
        }
        // ... erase file ...
        if ( cc::fs::posix::File::Exists(a_to_uri) ) {
            cc::fs::posix::File::Erase(a_to_uri);
        }
        // ... rethrow exception ...
        throw a_fs_exception;
    } catch (...) {
        // ... release previously allocated memory ...
        delete [] buffer;
        if ( nullptr != md5 ) {
            delete md5;
        }
        // ... erase file ...
        if ( cc::fs::posix::File::Exists(a_to_uri) ) {
            cc::fs::posix::File::Erase(a_to_uri);
        }
        // ... rethrow exception ...
        CC_EXCEPTION_RETHROW(/* a_unhandled*/ true);
    }

    delete [] buffer;
    
    if ( nullptr != md5 ) {
        delete md5;
    }

    src_file.Close();
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
