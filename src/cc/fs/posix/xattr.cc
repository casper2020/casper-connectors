/**
 * @file xattr.h
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

#include "cc/fs/posix/xattr.h"

#include "cc/fs/exception.h"

#include "cc/hash/md5.h"

#include <string.h> // strerror

#include <sys/xattr.h>

#include <utility> // std::pair
#include <set>

#ifdef __APPLE__

//
// FOR *xattr FUNCTION CALLS:
//
// position:
//
// Position specifies the offset within the extended attribute.
// In the current implementation, only the resource fork extended attribute makes use of this argument.
// For all others, position is reserved and should be set to zero.
//
// options:
//
// XATTR_NOFOLLOW  do not follow symbolic links.
//                 setxattr() normally sets attributes on the target of path if it is a symbolic link.
//                 With this option, setxattr() will act on the link itself.
//
// XATTR_CREATE    fail if the named attribute already exists.
//
// XATTR_REPLACE   fail if the named attribute does not exist.
//
// Failure to specify XATTR_REPLACE or XATTR_CREATE allows creation and replacement.
//

    #define LIST_X_ATTR(a_uri, a_buffer, a_size) \
        listxattr(a_uri, a_buffer, a_size, /* options */ 0)
    #define FLIST_X_ATTR(a_fd, a_buffer, a_size) \
        flistxattr(a_fd, a_buffer, a_size, /* options */ 0)
    #define GET_X_ATTR(a_uri, a_name, a_buffer, a_size) \
        getxattr(a_uri, a_name, a_buffer, a_size, /* position */ 0, /* a_options */ 0)
    #define FGET_X_ATTR(a_fd, a_name, a_buffer, a_size) \
        fgetxattr(a_fd, a_name, a_buffer, a_size, /* position */ 0, /* a_options */ 0)
    #define SET_X_ATTR(a_uri, a_name, a_value, a_size) \
        setxattr(a_uri, a_name, a_value, a_size, /* position */ 0, /* options */ 0)
    #define FSET_X_ATTR(a_fd, a_name, a_value, a_size) \
        fsetxattr(a_fd, a_name, a_value, a_size, /* position */ 0, /* options */ 0)
    #define REMOVE_X_ATTR(a_uri, a_name) \
        removexattr(a_uri, a_name, /* options */ 0);
    #define FREMOVE_X_ATTR(a_fd, a_name) \
        fremovexattr(a_fd, a_name, /* options */ 0);
    #define XATTR_DOES_NOT_EXISTS ENOATTR

#else

//
// FOR *xattr FUNCTION CALLS:
//
// flags:
//
// XATTR_CREATE    fail if the named attribute already exists.
//
// XATTR_REPLACE   fail if the named attribute does not exist.
//
// By default (no flags), the extended attribute will be created if need be, or will simply replace the value if the attribute exists.
//

    #define LIST_X_ATTR(a_uri, a_buffer, a_size) \
        listxattr(a_uri, a_buffer, a_size)
    #define FLIST_X_ATTR(a_fd, a_buffer, a_size) \
        flistxattr(a_fd, a_buffer, a_size)
    #define GET_X_ATTR(a_uri, a_name, a_buffer, a_size) \
        getxattr(a_uri, a_name, a_buffer, a_size)
    #define FGET_X_ATTR(a_fd, a_name, a_buffer, a_size) \
        fgetxattr(a_fd, a_name, a_buffer, a_size)
    #define SET_X_ATTR(a_uri, a_name, a_value, a_size) \
        setxattr(a_uri, a_name, a_value, a_size, /* flags */ 0)
    #define FSET_X_ATTR(a_fd, a_name, a_value, a_size) \
        fsetxattr(a_fd, a_name, a_value, a_size, /* flags */ 0)
    #define REMOVE_X_ATTR(a_uri, a_name) \
        removexattr(a_uri, a_name)
    #define FREMOVE_X_ATTR(a_fd, a_name) \
        fremovexattr(a_fd, a_name)
    #define XATTR_DOES_NOT_EXISTS ENODATA
#endif

/**
 * @brief Constructor for a previously open file.
 *
 * @param a_fd
 */
cc::fs::posix::XAttr::XAttr (const int& a_fd)
    : uri_(""), fd_(a_fd)
{
    /* empty */
}

/**
 * @brief Constructor to open a file.
 *
 * @param a_uri
 */
cc::fs::posix::XAttr::XAttr (const std::string& a_uri)
    : uri_(a_uri), fd_(-1)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::fs::posix::XAttr::~XAttr()
{
    /* empty */
}

/**
 * @brief Set an xattr value.
 *
 * @param a_name  xattr name.
 * @param a_value xattr value to write.
 */
void cc::fs::posix::XAttr::Set (const std::string& a_name, const std::string& a_value) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open set xattr - no file uri or fd is set!");
    }

    //
    // On success, 0 is returned.
    // On failure, -1 is returned and the global variable errno is set.
    //

    int rv;
    if ( 0 != uri_.length() ) {
        rv = SET_X_ATTR(uri_.c_str(), a_name.c_str(), a_value.c_str(), a_value.length());
    } else {
        rv = FSET_X_ATTR(fd_, a_name.c_str(), a_value.c_str(), a_value.length());
    }
    
    // ... failure?
    if ( -1 == rv ) {
        throw cc::fs::Exception("Unable to set xattr '%s' - %s!", a_name.c_str(), strerror(errno));
    }
}

/**
 * @brief Retrieve an xattr value.
 *
 * @param a_name  xattr name.
 * @param o_value xattr value read.
 */
void cc::fs::posix::XAttr::Get (const std::string& a_name, std::string& o_value) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open get xattr - no file uri or fd is set!");
    }

    //
    // On failure, -1 is returned and errno is set.
    // On success, a positive number is returned indicating the size of the extended attribute value.
    //
    // If buffer is not large enough ERANGE is returned - never the number of required bytes,
    // so we're using a first call to determine the required buffer length and a second to get the actual content.
    //
    
    char*  vb = nullptr;

    try {
        
        const char* const attr_name_c_str = a_name.c_str();
        
        ssize_t rv;
        if ( 0 != uri_.length() ) {

            // ... 1st call to calculate required buffer size
            rv = GET_X_ATTR(uri_.c_str(), attr_name_c_str, nullptr, 0);
            if ( -1 == rv ) {
                throw cc::fs::Exception("Unable to get xattr '%s' - %s!", attr_name_c_str, strerror(errno));
            }
            
            // ... allocate buffer ...
            vb                          = new char[( static_cast<size_t>(rv) + sizeof(char) )];
            vb[static_cast<size_t>(rv)] = '\0';
            
            // ... 2nd call to retrieve value ...
            rv = GET_X_ATTR(uri_.c_str(), attr_name_c_str, vb, static_cast<size_t>(rv));
            if ( -1 == rv ) {
                throw cc::fs::Exception("Unable to get xattr '%s' - %s!", attr_name_c_str, strerror(errno));
            }
            
        } else {
            
            // ... 1st call to calculate required buffer size
            rv = FGET_X_ATTR(fd_, attr_name_c_str, nullptr, 0);
            if ( -1 == rv ) {
                throw cc::fs::Exception("Unable to get xattr '%s' - %s!", attr_name_c_str, strerror(errno));
            }

            // ... allocate buffer ...
            vb                          = new char[( static_cast<size_t>(rv) + sizeof(char) )];
            vb[static_cast<size_t>(rv)] = '\0';

            // ... 2nd call to retrieve value ...
            rv = GET_X_ATTR(uri_.c_str(), attr_name_c_str, nullptr, 0);
            if ( -1 == rv ) {
                throw cc::fs::Exception("Unable to get xattr '%s' - %s!", attr_name_c_str, strerror(errno));
            }

        }
        
        o_value = vb;
        
        delete [] vb;
        
    } catch (const cc::Exception& a_cc_exception) {
        if ( nullptr != vb ) {
            delete [] vb;
        }
        throw a_cc_exception;
    }

}

/**
 * @brief Check of an xattr exists
 *
 * @param a_name  xattr name..
 */
bool cc::fs::posix::XAttr::Exists (const std::string& a_name) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open verify if xattr exists - no file uri or fd is set!");
    }
    
    //
    // On failure, -1 is returned and errno is set.
    // On success, a positive number is returned indicating the size of the extended attribute value.
    //
    
    const char* const attr_name = a_name.c_str();
        
    ssize_t rv;
    if ( 0 != uri_.length() ) {
        rv = GET_X_ATTR(uri_.c_str(), attr_name, nullptr, 0);
    } else {
        rv = FGET_X_ATTR(fd_, attr_name, nullptr, 0);
    }
    
    if ( -1 == rv ) {
        const int err_no = errno;
        if ( XATTR_DOES_NOT_EXISTS != err_no ) {
            throw cc::fs::Exception("Unable to open verify if xattr '%s' exists - %s!", attr_name, strerror(err_no));
        }
        return false;
    }

    return true;
}

/**
 * @brief Removes an xattr.
 *
 * @param a_name xattr name.
 * @param o_value xattr value ( optional )
 */
void cc::fs::posix::XAttr::Remove (const std::string& a_name, std::string* o_value) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open remove xattr - no file uri or fd is set!");
    }

    //
    // On success, 0 is returned.
    // On failure, -1 is returned and the global variable errno is set.
    //

    // ... if required, fetch current value ...
    if ( nullptr != o_value ) {
        Get(a_name, *o_value);
    }
    
    // ... now remove it ...
    int rv;
    if ( 0 != uri_.length() ) {
        rv = REMOVE_X_ATTR(uri_.c_str(), a_name.c_str());
    } else {
        rv = FREMOVE_X_ATTR(fd_, a_name.c_str());
    }

    // ... failure?
    if ( -1 == rv ) {
        throw cc::fs::Exception("Unable to remove xattr '%s' - %s!", a_name.c_str(), strerror(errno));
    }
}

/**
 * @brief Iterate all extended attributes.
 *
 * @param a_callback Function to call to deliver each iteration entry.
 */
void cc::fs::posix::XAttr::Iterate (const std::function<void(const char* const, const char* const)>& a_callback) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open iterate xattrs - no file uri or fd is set!");
    }
    
    //
    // On failure, -1 is returned and errno is set.
    // On success, a positive number is returned indicating the size of the extended attribute value.
    //
    // If buffer is not large enough ERANGE is returned - never the number of required bytes,
    // so we're using a first call to determine the required buffer length and a second to get the actual content.
    //
    
    char* kb = nullptr;
    char* vb = nullptr;
    
    try {
        
        //
        // GRAB XATTR KEYS:
        //
        
        // ... calculate the length of the buffer needed ...
        ssize_t kb_l;
        if ( 0 != uri_.length() ) {
            kb_l = LIST_X_ATTR(uri_.c_str(), /* buffer */ nullptr, /* size */ 0);
        } else {
            kb_l = FLIST_X_ATTR(fd_, /* buffer */ nullptr, /* size */ 0);
        }
        if ( -1 == kb_l ) {
            throw cc::fs::Exception("Unable to list xattrs - %s!", strerror(errno));
        } else if ( 0 == kb_l ) {
            return;
        }

        kb = new char[static_cast<size_t>(kb_l)];
        
        // ... copy list of xattr keys ...
        if ( 0 != uri_.length() ) {
            kb_l = LIST_X_ATTR(uri_.c_str(), kb, static_cast<size_t>(kb_l));
        } else {
            kb_l = FLIST_X_ATTR(fd_, kb, static_cast<size_t>(kb_l));
        }
        
        if ( -1 == kb_l ) {
            throw cc::fs::Exception("Unable to list xattrs - %s!", strerror(errno));
        }
        
        //
        // GRAB XATTR VALUE FOR EACH KEY:
        //
        const char* kn = kb;
        size_t      kl = 0;

        // ... iterate keys buffer ( zero terminated strings are provided ) ...
        ssize_t  rv = 0;
        if ( 0 != uri_.length() ) {
            
            const char* const uri = uri_.c_str();
            while ( kb_l > 0 ) {
                
                // ... 1st call to calculate required buffer size
                rv = GET_X_ATTR(uri, kn, nullptr, 0);
                if ( -1 == rv ) {
                    throw cc::fs::Exception("Unable to get xattr '%s' - %s!", kn, strerror(errno));
                }
                
                // ... allocate buffer ...
                vb                          = new char[( static_cast<size_t>(rv) + sizeof(char) )];
                vb[static_cast<size_t>(rv)] = '\0';
                
                // ... 2nd call to retrieve value ...
                rv = GET_X_ATTR(uri, kn, vb, static_cast<size_t>(rv));
                if ( -1 == rv ) {
                    throw cc::fs::Exception("Unable to get xattr '%s' - %s!", kn, strerror(errno));
                }
                
                // ... notify ...
                a_callback(kn, vb);
                
                // ... forget it ...
                delete [] vb;
                vb = nullptr;
                
                // ... next ...
                kl    = ( strlen(kn) + sizeof(char) );
                kn   += kl;
                kb_l -= kl;
            }
            
        } else {
            
            while ( kb_l > 0 ) {
                
                // ... 1st call to calculate required buffer size
                rv = FGET_X_ATTR(fd_, kn, nullptr, 0);
                if ( -1 == rv ) {
                    throw cc::fs::Exception("Unable to get xattr '%s' - %s!", kn, strerror(errno));
                }
                
                // ... allocate buffer ...
                vb                          = new char[( static_cast<size_t>(rv) + sizeof(char) )];
                vb[static_cast<size_t>(rv)] = '\0';
                
                // ... 2nd call to retrieve value ...
                rv = FGET_X_ATTR(fd_, kn, vb, static_cast<size_t>(rv));
                if ( -1 == rv ) {
                    throw cc::fs::Exception("Unable to get xattr '%s' - %s!", kn, strerror(errno));
                }
                
                // ... notify ...
                a_callback( kn, vb);
                
                // ... forget it ...
                delete [] vb;
                vb = nullptr;
                
                // ... next ...
                kl    = ( strlen(kn) + sizeof(char) );
                kn   += kl;
                kb_l -= kl;
            }
            
        }
        
        delete [] kb;

    } catch (const cc::Exception& a_cc_exception) {
        if ( nullptr != kb ) {
            delete [] kb;
        }
        if ( nullptr != vb ) {
            delete [] vb;
        }
        throw a_cc_exception;
    }
}

/**
 * @brief Calculate and save attributes seal.
 *
 * @param a_name
 * @param a_magic
 * @param a_length
 * @param a_excluding_attrs Set of xattrs name to exlcude.
 */
void cc::fs::posix::XAttr::Seal (const std::string& a_name, const unsigned char* a_magic, size_t a_length,
                                 const std::set<std::string>* a_excluding_attrs) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open calculate xattrs seal - no file uri or fd is set!");
    }
    
    cc::hash::MD5 md5;
    md5.Initialize();
    IterateOrdered([&md5, a_name, &a_excluding_attrs] (const char* const a_key, const char *const a_value) {
        if ( ( nullptr == a_excluding_attrs || ( a_excluding_attrs->end() == a_excluding_attrs->find(a_key) ) )
            &&
            0 != strcasecmp(a_key, a_name.c_str())
            ) {
            
            md5.Update(reinterpret_cast<const unsigned char*>(a_key), strlen(a_key));
            md5.Update(reinterpret_cast<const unsigned char*>("&"), sizeof(char));
            md5.Update(reinterpret_cast<const unsigned char*>(a_value), strlen(a_value));
        }
    });
    
    const std::string tmp = md5.Finalize();
    
    char seal [65] = { '\0' };
    size_t r = ( sizeof(seal) / sizeof(seal[0]) );
    for ( size_t idx = 0; idx < tmp.length() ; idx++ ) {
        snprintf(&(seal[idx*2]), r, "%02x", ( tmp[idx] ^ a_magic[idx % a_length] ));
        r -= 2;
    }
    seal[64] = '\0';

    Set(a_name, seal);
}

void cc::fs::posix::XAttr::Seal (const std::string& a_name, const std::set<std::string>& a_attrs,
                                 const unsigned char* a_magic, size_t a_length) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to open calculate xattrs seal - no file uri or fd is set!");
    }
    
    cc::hash::MD5 md5;
    md5.Initialize();
    IterateOrdered([&md5, a_name, &a_attrs] (const char* const a_key, const char *const a_value) {
        if ( a_attrs.end() != a_attrs.find(a_key) && 0 != strcasecmp(a_key, a_name.c_str()) ) {
            md5.Update(reinterpret_cast<const unsigned char*>(a_key), strlen(a_key));
            md5.Update(reinterpret_cast<const unsigned char*>("&"), sizeof(char));
            md5.Update(reinterpret_cast<const unsigned char*>(a_value), strlen(a_value));
        }
    });
    
    const std::string tmp = md5.Finalize();
    
    char seal [65] = { '\0' };
    size_t r = ( sizeof(seal) / sizeof(seal[0]) );
    for ( size_t idx = 0; idx < tmp.length() ; idx++ ) {
        snprintf(&(seal[idx*2]), r, "%02x", ( tmp[idx] ^ a_magic[idx % a_length] ));
        r -= 2;
    }
    seal[64] = '\0';

    Set(a_name, seal);
}

/**
 * @brief Calculate and validate attributes seal.
 */
void cc::fs::posix::XAttr::Validate (const std::string& a_name, const unsigned char* a_magic, size_t a_length,
                                     const std::set<std::string>* a_excluding_attrs) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to verify xattrs seal - no file uri or fd is set!");
    }

    std::string value;
    Get(a_name, value);

    cc::hash::MD5 md5;
    md5.Initialize();
    IterateOrdered([&md5, a_name, &a_excluding_attrs] (const char *const a_key, const char *const a_value) {
        if ( ( nullptr == a_excluding_attrs || ( a_excluding_attrs->end() == a_excluding_attrs->find(a_key) ) )
              &&
              0 != strcasecmp(a_key, a_name.c_str())
        ) {
            md5.Update(reinterpret_cast<const unsigned char*>(a_key), strlen(a_key));
            md5.Update(reinterpret_cast<const unsigned char*>("&"), sizeof(char));
            md5.Update(reinterpret_cast<const unsigned char*>(a_value), strlen(a_value));
        }
    });

    const std::string tmp = md5.Finalize();
    
    char seal [65] = { 0, 0 };
    size_t r = ( sizeof(seal) / sizeof(seal[0]) );
    for ( size_t idx = 0; idx < tmp.length() ; idx++ ) {
        snprintf(&(seal[idx*2]), r, "%02x", ( tmp[idx] ^ a_magic[idx % a_length] ));
        r -= 2;
    }
    seal[64] = '\0';

    if ( 0 != value.compare(seal) ) {
        throw cc::fs::Exception("%s tampered!", a_name.c_str());
    }
}

/**
 * @brief Calculate and validate attributes seal for a specific set of attributes.
 */
void cc::fs::posix::XAttr::Validate (const std::string& a_name, const std::set<std::string>& a_attrs,
                                     const unsigned char* a_magic, size_t a_length) const
{
    // ... ensure valid 'access' to file ...
    if ( 0 == uri_.length() && -1 == fd_ ) {
        throw cc::fs::Exception("Unable to verify xattrs seal - no file uri or fd is set!");
    }

    std::string value;
    Get(a_name, value);

    cc::hash::MD5 md5;
    md5.Initialize();
    IterateOrdered([&md5, a_name, &a_attrs] (const char *const a_key, const char *const a_value) {
        if ( a_attrs.end() != a_attrs.find(a_key) && 0 != strcasecmp(a_key, a_name.c_str() ) ) {
            md5.Update(reinterpret_cast<const unsigned char*>(a_key), strlen(a_key));
            md5.Update(reinterpret_cast<const unsigned char*>("&"), sizeof(char));
            md5.Update(reinterpret_cast<const unsigned char*>(a_value), strlen(a_value));
        }
    });

    const std::string tmp = md5.Finalize();
    
    char seal [65] = { 0, 0 };
    size_t r = ( sizeof(seal) / sizeof(seal[0]) );
    for ( size_t idx = 0; idx < tmp.length() ; idx++ ) {
        snprintf(&(seal[idx*2]), r, "%02x", ( tmp[idx] ^ a_magic[idx % a_length] ));
        r -= 2;
    }
    seal[64] = '\0';

    if ( 0 != value.compare(seal) ) {
        throw cc::fs::Exception("%s tampered!", a_name.c_str());
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Iterate all extended attributes, ordered by name.
 *
 * @param a_callback Function to call to deliver each iteration entry.
 */
void cc::fs::posix::XAttr::IterateOrdered (const std::function<void(const char* const, const char* const)>& a_callback) const
{
    std::map<std::string, std::string> attrs;

    Iterate([&attrs] (const char* const a_key, const char *const a_value) {
        attrs[a_key] = a_value;
    });
    
    for ( auto e : attrs ) {
        a_callback(e.first.c_str(), e.second.c_str());
    }
}

/**
 * @brief Iterate all extended attributes filtered by regular expression and ordered by name .
 *
 * @param a_expr     Regular expression.
 * @param a_callback Function to call to deliver each iteration entry.
 */
void cc::fs::posix::XAttr::IterateOrdered (const std::regex& a_expr, const std::function<void(const char* const, const char* const)>& a_callback) const
{
    std::map<std::string, std::string> attrs;

    Iterate([&attrs, &a_expr] (const char* const a_key, const char *const a_value) {
        if ( true == std::regex_match(a_key, a_expr) ) {
            attrs[a_key] = a_value;
        }
    });
    
    for ( auto e : attrs ) {
        a_callback(e.first.c_str(), e.second.c_str());
    }
}
