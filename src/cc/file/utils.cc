/**
 * @file utils.cc
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
#include "cc/file/utils.h"

#include "cc/file/exception.h"

#include <unistd.h> // mkstemps, unlink
#include <limits.h> // PATH_MAX
#include <string.h> // memcpy
#include <sys/stat.h> // stat

/**
 * @brief Ensure a unique URI for a file.
 *
 * @param a_directory
 * @param a_file_name
 * @param a_file_extension
 *
 * @param o_uri
 */
void cc::file::Utils::EnsureUnique (const std::string& a_directory, const std::string& a_file_name, const std::string& a_file_extension,
                                    std::string& o_uri)
{
    if ( a_file_name.length() > 0 ) {
        o_uri = a_directory + a_file_name + ".XXXXXX." + a_file_extension;
    } else {
        o_uri = a_directory + "XXXXXX." + a_file_extension;
    }
    
    char f_template[PATH_MAX] = { 0, 0 };
    memcpy(f_template, o_uri.c_str(), o_uri.length());
    int fd = mkstemps(f_template, sizeof(char) * static_cast<int>(a_file_extension.length() + 1));
    if ( -1 == fd ) {
        throw cc::Exception("%d - %s", errno, strerror(errno));
    } else {
        close(fd);
    }
    
    o_uri = f_template;
}

/**
 * @brief Erase a file
 *
 * @param a_uri
 *
 * @return
 */
bool cc::file::Utils::Exists (const std::string& a_uri)
{
    struct stat stat_info;
    if ( 0 == stat(a_uri.c_str(), &stat_info) ) {
        return ( 0 != S_ISREG(stat_info.st_mode) );
    }
    const int ec = errno;
    if ( ENOENT == ec ) {
        return false;
    }
    throw cc::Exception("%d - %s", ec, strerror(ec));
}

/**
 * @brief Erase a file
 *
 * @param a_uri
 */
void cc::file::Utils::Erase (const std::string& a_uri)
{
    const int rv = unlink(a_uri.c_str());
    if ( 0 != rv ) {
        throw cc::Exception("%d - %s", errno, strerror(errno));
    }
}
