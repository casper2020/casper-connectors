/**
 * @file dir.h
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
#ifndef NRS_CC_FS_POSIX_DIR_H_
#define NRS_CC_FS_POSIX_DIR_H_

#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

namespace cc
{
    
    namespace fs
    {
        
        namespace posix
        {
            
            class Dir final : public NonCopyable, public NonMovable
            {
                
            public: // Static Data
                
                static constexpr mode_t k_default_mode_ = ( S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH );
                
            private: // Const Data
                
                const std::string path_;
                
            public: // Constructor(s) / Destructor
                
                Dir () = delete;
                Dir (const std::string& a_path);
                virtual ~Dir();
                
            public: // Method(s) / Function(s)
                
                bool Exists () const;
                void Make   (const mode_t a_mode = k_default_mode_) const;
                
            public: // Status Method(s) / Function(s)
                
                static std::string Normalize             (const std::string& a_path);
                static std::string Normalize             (const char* const a_path);
                static bool        Exists                (const std::string& a_path);
                static bool        Exists                (const char* const a_path);
                static void        Make                  (const std::string& a_path, const mode_t a_mode = k_default_mode_);
                static void        Make                  (const char* const a_path, const mode_t a_mode = k_default_mode_);
                static void        Parent                (const std::string& a_path, std::string& o_path);
                static void        Parent                (const char* const a_path, std::string& o_path);
                static void        EnsureEnoughFreeSpace (const std::string& a_path, size_t a_required,
                                                          const char* const a_error_msg_prefix = nullptr);
                static void        EnsureEnoughFreeSpace (const char* const a_path, size_t a_required,
                                                          const char* const a_error_msg_prefix = nullptr);
                static void        Expand                (const std::string& a_uri, std::string& o_uri);
                static std::string Expand                (const std::string& a_uri);
                static std::string RealPath              (const std::string& a_path);
                static std::string ReadLink              (const std::string& a_path);
                
                static void        ListFiles (const std::string& a_path, const std::string& a_pattern,
                                              const std::function<bool(const std::string& a_uri)> a_callback);
                
            }; // end of class 'dir'
            
        } // end of namespace 'posix'
        
    } // end of namespace 'fs'
    
} // end of namespace 'cc'

#endif // NRS_CC_FS_POSIX_DIR_H_
