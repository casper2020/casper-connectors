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
#ifndef NRS_CC_FS_FILE_H_
#define NRS_CC_FS_FILE_H_

#include "cc/fs/posix/file.h"
#include "cc/fs/posix/xattr.h"

namespace cc
{
    
    namespace fs
    {

        //              //
        // 'File' class //
        //              //
        typedef cc::fs::posix::File File;
        
        //                  //
        // 'file' namespace //
        //                  //
        namespace file
        {

            //               //
            // 'XAttr' class //
            //               //
            typedef cc::fs::posix::XAttr XAttr;

            //                //
            // 'Writer' class //
            //                //
            class Writer final : public File
            {
                
            public: // Constructor(s) / Destructor
                
                Writer ();
                virtual ~Writer();
                
            }; // end of class 'Writer'

            //               //
            // 'Reader' class //
            //               //
            class Reader final : public File
            {
                
            public: // Constructor(s) / Destructor
                
                Reader ();
                virtual ~Reader();
                
            }; // end of class 'Reader'

        } // end of namespace 'file'
        
    } // end of namespace 'fs'
    
} // end of namespace 'cc'

#endif // NRS_CC_FS_FILE_H_

