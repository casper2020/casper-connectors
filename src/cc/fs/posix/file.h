/**
 * @file file.h
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
#ifndef NRS_CC_FS_POSIX_FILE_H_
#define NRS_CC_FS_POSIX_FILE_H_

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

            class File : public NonCopyable, public NonMovable
            {
                
            public: // Enum(s)
                
                enum class Mode : uint8_t {
                    NotSet,
                    Read,
                    Write,
                    Append
                };
                
            private: // Data

                Mode        mode_;
                FILE*       fp_;
                std::string uri_;
                
            public: // Constructor(s) / Destructor
                
                File ();
                virtual ~File();
                
            public: // Method(s) / Function(s)
                
                virtual void     Open  (const std::string& a_uri, const Mode& a_mode);
                virtual size_t   Read (unsigned char* o_data, const size_t a_size, bool& o_eof);
                virtual void     Open  (const std::string& a_path, const std::string& a_prefix, const std::string& a_extension, const size_t& a_size);
                virtual size_t   Write (const unsigned char* a_data, const size_t a_size, const bool a_flush = false);
                virtual size_t   Write (const std::string& a_data, const bool a_flush = false);
                virtual void     Seek  (const size_t& a_pos);
                virtual void     Flush ();
                virtual void     Close (const bool a_force = false);
                virtual uint64_t Size  ();

            public: // Inline Method(s) / Function(s)
                
                const std::string& URI    () const;
                bool               IsOpen () const;
                
            public: // Status Method(s) / Function(s)
                
                static void     Name      (const std::string& a_uri, std::string& o_name);
                static void     Extension (const std::string& a_uri, std::string& o_extension);
                static void     Path      (const std::string& a_uri, std::string& o_path);
                static bool     Exists    (const std::string& a_uri);
                static void     Erase     (const std::string& a_uri);
                static void     Erase     (const std::string& a_dir, const std::string& a_pattern);
                static void     Rename    (const std::string& a_from_uri, const std::string& a_to_uri);
                static void     Copy      (const std::string& a_from_uri, const std::string& a_to_uri,
                                           const bool a_overwrite = false, std::string* os_md5 = nullptr);
                static uint64_t Size      (const std::string& a_uri);
                static void     Unique    (const std::string& a_path, const std::string& a_name, const std::string& a_extension,
                                           std::string& o_uri);
                
            }; // end of class 'File'
            
            /**
             * @return The file URI.
             */
            inline const std::string& File::URI () const
            {
                return uri_;
            }
            
            /**
             * @return The file URI.
             */
            inline bool File::IsOpen () const
            {
                return ( nullptr != fp_ );
            }

        } // end of namespace 'posix'
        
    } // end of namespace 'fs'
    
} // end of namespace 'cc'

#endif // NRS_CC_FS_POSIX_FILE_H_
