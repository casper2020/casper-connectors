/**
 * @file writer.h
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
#pragma once
#ifndef NRS_CC_WRITER_H_
#define NRS_CC_WRITER_H_

#include <stdio.h>

#include <string>

namespace cc
{
    
    namespace file
    {
        
        class Writer
        {
            
        protected: //
            
            FILE*       fp_;
            std::string uri_;
            
        public: // Constructor(s) / Destructor
            
            Writer ();
            virtual ~Writer();
            
        public: // Method(s) / Function(s)
            
            virtual void   Open  (const std::string& a_uri);
            virtual size_t Write (const unsigned char* a_data, const size_t a_size, const bool a_flush = false);
            virtual void   Close (const bool a_force = false);
            
        public: // Inline Method(s) / Function(s)
            
            const std::string& URI() const;
            
        }; // end of class 'Writer'
        
        /**
         * @return The file URI.
         */
        inline const std::string& Writer::URI () const
        {
            return uri_;
        }
        
    } // end of namespace 'file'
    
} // end of namespace 'cc'

#endif // NRS_CC_FILE_WRITER_H_
