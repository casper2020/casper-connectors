/**
 * @file mime_type.h
 *
 * Copyright (c) 2017-2023 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef CC_MAGIC_MIME_TYPE_H_
#define CC_MAGIC_MIME_TYPE_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <set>
#include <map>
#include <string>

#include "cc/pragmas.h"

CC_DIAGNOSTIC_PUSH()
CC_DIAGNOSTIC_IGNORED("-Wc++98-compat-extra-semi")
#include "magic.h"
CC_DIAGNOSTIC_POP()

namespace cc
{

    namespace magic
    {
        
        class MIMEType final : public cc::NonCopyable, public cc::NonMovable
        {
            
        private: // Const Data
            
            static const std::string sk_pdf_;
            
        private: // Data
            
            magic_t       cookie_;
            unsigned char buffer_[1024];
            
        public: // Constructor(s) / Destructor
            
            MIMEType ();
            virtual ~MIMEType();
            
        public: // API Method(s) / Function(s)
            
            void        Initialize (const std::string& a_shared_directory, int a_flags = MAGIC_MIME_TYPE);
            void        Reset      (const int a_flags);
            std::string MIMETypeOf (const std::string& a_uri) const;
            std::string MIMETypeOf (const std::string& a_uri, std::size_t& o_offset);
            
            std::string WithoutCharsetOf(const std::string& a_uri);
            
        }; // end of class 'MIMEType'
        
    } // end of namespace 'magic'

} // end of namespace 'cc'

#endif // CC_MAGIC_MIME_TYPE_H_
