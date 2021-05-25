/**
 * @file dir.cc
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

#include "cc/fs/posix/dir.h"

#include "cc/fs/exception.h"

#include <sys/errno.h>
#include <stdio.h>

#include <libgen.h> // dirname

#include <string.h> // strlen, strerror, ...

#include <unistd.h> // getuid, readlink
#include <pwd.h> // getpwuid

// statfs & PATH_PAMX
#ifdef __APPLE__
  #include <sys/param.h>
  #include <sys/mount.h>
  #include <sys/syslimits.h>
#else // linux
  #include <sys/vfs.h>    /* or <sys/statfs.h> */
  #include <linux/limits.h>
#endif

/**
 * @brief Default constructor.
 *
 * @param a_path Directory full path.
 */
cc::fs::posix::Dir::Dir (const std::string& a_path)
    : path_(a_path)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::fs::posix::Dir::~Dir ()
{
    /* empty */
}

/**
 * @brief Check if directory exists.
 *
 * @return True if exists, false otherwise.
 */
bool cc::fs::posix::Dir::Exists () const
{
    return Exists(path_.c_str());
}

/**
 * @brief Create (a) directory(vs) follwing a path.
 *
 * param a_mode
 */
void cc::fs::posix::Dir::Make (const mode_t a_mode) const
{
    Make(path_.c_str(), a_mode);
}

#ifdef __APPLE__
#pragma mark - STATIC METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Normalize a path.
 *
 * @param a_path Directory full path.
 *
 * @return Normalized path.
 */
std::string cc::fs::posix::Dir::Normalize (const std::string& a_path)
{
    return Normalize(a_path.c_str());
}

/**
 * @brief Normalize a path.
 *
 * @param a_path Directory full path.
 *
 * @return Normalized path.
 */
std::string cc::fs::posix::Dir::Normalize (const char* const a_path)
{
    const size_t str_len = nullptr != a_path ? strlen(a_path) : 0;
    if ( str_len > 0 && '/' != a_path[str_len - sizeof(char)] ) {
        return ( std::string(a_path) + '/' );
    }
    return a_path;
}

/**
 * @brief Check if a directory exists.
 *
 * @param a_path Directory full path.
 */
bool cc::fs::posix::Dir::Exists (const std::string& a_path)
{
    return Exists(a_path.c_str());
}

/**
 * @brief Check if a directory exists.
 *
 * @param a_path Directory full path.
 */
bool cc::fs::posix::Dir::Exists (const char* const a_path)
{
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    if ( stat(a_path, &st) == 0 ) {
        return ( ( st.st_mode & S_IFDIR ) == S_IFDIR );
    }
    const int ec = errno;
    if ( ENOENT == ec ) {
        return false;
    }
    throw cc::fs::Exception("Unable to check if directory '%s' exists - %s!", a_path, strerror(errno));
}

/**
 * @brief Create (a) directory(vs) follwing a path.
 *
 * @param a_path Directory full path.
 * param a_mode
 */
void cc::fs::posix::Dir::Make (const std::string& a_path, const mode_t a_mode)
{
    return Make (a_path.c_str(), a_mode);
}

/**
 * @brief Create (a) directory(vs) follwing a path.
 *
 * @param a_path Directory full path.
 * param a_mode
 */
void cc::fs::posix::Dir::Make (const char* const a_path, const mode_t a_mode)
{
    // ... ensure PATH_MAX is respected ...
    if ( strlen(a_path) > ( PATH_MAX - sizeof(char)) ) {
        throw cc::fs::Exception("Unable to create path '%s': max path overflow!", a_path);
    }
    
    // ... ensure we've a valid path ...
    if ( '\0' == a_path[0] ) {
        throw cc::fs::Exception("Unable to create path '%s': invalid path!", a_path);
    }
    
    char        uri [PATH_MAX];
    size_t      len;
    size_t      adv = 0;
    
    // ... create path ( if does not exist ) ...
    const char* lst = a_path;
    const char* nxt = strchr(a_path + sizeof(char), '/');
    while ( nullptr != nxt && '\0' != nxt[0] ) {
        // ... calculate current dir name length ...
        len = ( nxt - lst );
        // ... copy current dir name ...
        uri[len + adv] = '\0';
        memcpy(uri + adv, lst, len);
        // ... create directory ...
        if ( -1 == mkdir(uri, a_mode) ) {
            const int no = errno;
            if ( no != EEXIST ) {
                throw cc::fs::Exception("Unable to create directory '%s': %s!", uri, strerror(no));
            }
        }
        // ... next ...
        adv += len;
        lst  = nxt;
        nxt  = strchr(nxt + sizeof(char), '/');
    }
    // ... last path component ( when last '/' is missing from a_uri ) ...
    if ( nullptr != lst ) {
        len = ( strlen(a_path) - ( lst - a_path ) );
        if ( len > 0 && '/' != lst[len - sizeof(char)] ) {
            if ( -1 == mkdir(a_path, a_mode) ) {
                const int no = errno;
                if ( no != EEXIST ) {
                    throw cc::fs::Exception("Unable to create directory '%s': %s!", a_path, strerror(no));
                }
            }
        }
    }
}

/**
 * @brief Extract parent directory from an path
 *
 * @param a_path The directory path.
 * @param o_path The parent directory path.
 */
void cc::fs::posix::Dir::Parent (const std::string& a_path, std::string& o_path)
{
    Parent(a_path.c_str(), o_path);
}

/**
 * @brief Extract parent directory from an path
 *
 * @param a_path The directory path.
 * @param o_path The parent directory path.
 */
void cc::fs::posix::Dir::Parent (const char* const a_path, std::string& o_path)
{
    const size_t l1 = strlen(a_path);
    if ( l1 >= ( PATH_MAX - ( sizeof(char) * 3 ) ) ) {
        throw cc::fs::Exception("Unable to obtain parent from path '%s': max path overflow!", a_path);
    }
    
    char path[PATH_MAX];
    
    memcpy(path, a_path, l1);
    memcpy((&path[l1 - sizeof(char)]), "../", sizeof(char) * 3);
    path[l1 + ( sizeof(char) * 2 )] = 0;
    
    const char* const ptr = dirname(path);
    
    const size_t l2 = strlen(ptr);
    if ( 0 == l2 || ( 1 == l2 && '/' == ptr[l2-1] ) || ( 1 == l2 && '.' == ptr[l2-1] ) || ( 2 == l2 && '.' == ptr[l2-1] && '.' == ptr[l2-2] ) ) {
        throw cc::fs::Exception("Unable to obtain parent from path '%s': is not a valid file name!", ptr);
    }
    
    o_path = Normalize(ptr);
}

/**
 * @brief Ensure there is enough free space ( but we're not reserving it ).
 *
 * @param a_path             Directory full path.
 * @param a_required         Size in bytes.
 * @param a_error_msg_prefix Error message prefix.
 */
void cc::fs::posix::Dir::EnsureEnoughFreeSpace (const std::string& a_path, size_t a_required,
                                                const char* const a_error_msg_prefix)
{
    EnsureEnoughFreeSpace(a_path.c_str(), a_required, a_error_msg_prefix);
}

/**
 * @brief Ensure there is enough free space ( but we're not reserving it ).
 *
 * @param a_path             Directory full path.
 * @param a_required         Size in bytes.
 * @param a_error_msg_prefix Error message prefix.
 */
void cc::fs::posix::Dir::EnsureEnoughFreeSpace (const char* const a_path, size_t a_required,
                                                const char* const a_error_msg_prefix)
{
    std::stringstream ss;
    
    uint64_t available_space = 0;
    struct statfs stat_data;
    if ( -1 == statfs(a_path, &stat_data) ) {
        if ( nullptr != a_error_msg_prefix ) {
            ss << a_error_msg_prefix << ": u";
        } else {
            ss << "U";
        }
        ss << "nable to check if there is enough free space to write file to directory '" << a_path << "': " << strerror(errno) << "!";
        throw cc::fs::Exception(ss.str());
    }
    available_space = (uint64_t)((uint64_t)stat_data.f_bfree * (uint64_t)stat_data.f_bsize);
    if ( available_space <= static_cast<uint64_t>(a_required) ) {
        if ( nullptr != a_error_msg_prefix ) {
            ss << a_error_msg_prefix << ": n";
        } else {
            ss << "N";
        }
        ss << "ot enougth free space to write file to directory '" << a_path << "':";
        ss << " required " << a_required << " bytes";
        ss << " but there are only " << available_space << " bytes available!";
        throw cc::fs::Exception(ss.str());
    }
}

/**
 * @brief Expand a 'short' path into a full path ( also supports ~ replacement ).
 *
 * @param a_path Short path.
 * @param o_path Expanded URI.
 */
void cc::fs::posix::Dir::Expand (const std::string& a_path, std::string& o_path)
{
    char  buff[PATH_MAX] = { 0, 0 };
    
    const char* c_str = a_path.c_str();
    if ( '~' == c_str[0] ) {
        struct passwd *pw = getpwuid(getuid());
        o_path = pw->pw_dir + std::string(c_str + sizeof(char));
    } else {
        char* ptr = realpath(a_path.c_str(), buff);
        if ( nullptr == ptr ) {
            throw ::cc::Exception("An error occurred while trying to obtain full path: (%d) %s ", errno, strerror(errno));
        } else {
            o_path = ptr;
        }
    }
}

/**
 * @brief Expand a 'short' path into a full path ( also supports ~ replacement ).
 *
 * @param a_uri Short path.
 *
 * @return Expanded URI.
 */
std::string cc::fs::posix::Dir::Expand (const std::string& a_uri)
{
    char  buff[PATH_MAX] = { 0, 0 };
    
    const char* c_str = a_uri.c_str();
    if ( '~' == c_str[0] ) {
        struct passwd *pw = getpwuid(getuid());
        return pw->pw_dir + std::string(c_str + sizeof(char));
    } else {
        char* ptr = realpath(a_uri.c_str(), buff);
        if ( nullptr == ptr ) {
            throw ::cc::Exception("An error occurred while trying to obtain full path: (%d) %s ", errno, strerror(errno));
        } else {
            return ptr;
        }
    }
}

/**
 * @brief Calculate the canonicalized absolute pathname.
 *
 * @param a_path Path.
 *
 * @return The canonicalized absolute pathname
 */
std::string cc::fs::posix::Dir::RealPath (const std::string& a_path)
{
    return Expand(a_path);
}

/**
 * @brief Calculate the canonicalized absolute pathname.
 *
 * @param a_path Path.
 *
 * @return The canonicalized absolute pathname
 */
std::string cc::fs::posix::Dir::ReadLink (const std::string& a_path)
{
    char buffer[PATH_MAX];
    ssize_t len;
    if ( -1 == ( len = readlink(a_path.c_str(), buffer, PATH_MAX-1) ) ) {
        throw ::cc::Exception("An error occurred while trying to obtain real path: (%d) %s ", errno, strerror(errno));
    } else if ( (PATH_MAX-1-1) == len ) {
        throw ::cc::Exception("An error occurred while trying to obtain real path: (%d) %s ", PATH_MAX, "buffer to short to write URI");
    } else {
        return std::string(buffer, len);
    }
}
