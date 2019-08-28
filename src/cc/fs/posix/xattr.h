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
#pragma once
#ifndef NRS_CC_FS_POSIX_XATTR_H_
#define NRS_CC_FS_POSIX_XATTR_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <string>
#include <map>
#include <set>
#include <functional>

#define XATTR_ARCHIVE_PREFIX "user."

namespace cc
{
    
    namespace fs
    {
        
        namespace posix
        {
        
            class XAttr final : public NonCopyable, NonMovable
            {
                
            private: // Const Data
                
                const std::string uri_;
                
            private: // Data
                
                const int fd_;
                
            public: // Constructor(s) / Destructor
                
                XAttr () = delete;
                XAttr (const int& a_fd);
                XAttr (const std::string& a_uri);
                virtual ~XAttr();
                
            public: // Method(s) / Function(s)
                
                void Set      (const std::string& a_name, const std::string& a_value) const;
                void Get      (const std::string& a_name, std::string& o_value) const;
                bool Exists   (const std::string& a_name) const;
                void Remove   (const std::string& a_name, std::string* o_value = nullptr) const;
                void Iterate  (const std::function<void(const char* const, const char* const)>& a_callback) const;
                void Seal     (const std::string& a_name, const unsigned char* a_magic, size_t a_length,
                               const std::set<std::string>* a_excluding_attrs) const;
                void Seal     (const std::string& a_name, const std::set<std::string>& a_attrs,
                               const unsigned char* a_magic, size_t a_length) const;
                void Validate (const std::string& a_name, const unsigned char* a_magic, size_t a_length,
                               const std::set<std::string>* a_excluding_attrs) const;
                void Validate (const std::string& a_name, const std::set<std::string>& a_attrs,
                               const unsigned char* a_magic, size_t a_length) const;

            public:
                
                void IterateOrdered (const std::function<void(const char* const, const char* const)>& a_callback) const;
                
            }; // end of class 'XAttr'
            
        } // end of namespace 'posix'
        
    } // end of namespace 'fs'
    
} // end of namespace 'cc'

#endif // NRS_CC_FS_POSIX_XATTR_H_
