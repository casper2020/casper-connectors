/**
* @file initializer.h
*
* Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors..
*
* casper-connectors. is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-connectors.  is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-connectors..  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#ifndef NRS_CC_ICU_INITIALIZER_H_
#define NRS_CC_ICU_INITIALIZER_H_

#include "cc/singleton.h"

#include <string>

#include <unicode/utypes.h> // UErrorCode

namespace cc
{

    namespace icu
    {
    
        // ---- //
        class Initializer;
        class OneShot final : public ::cc::Initializer<Initializer>
        {
            
        public: // Constructor(s) / Destructor
            
            OneShot (::cc::icu::Initializer& a_instance);
            virtual ~OneShot ();
            
        }; // end of class 'OneShot'
        
        // ---- //
        class Initializer final : public cc::Singleton<Initializer, OneShot>
        {
            
            friend class OneShot;
                
        private: // Data
            
            bool        initialized_;
            UErrorCode  last_error_code_;
            std::string load_error_msg_;

        public: // Method(s) / Function(s)
                
            const UErrorCode& Load (const std::string& a_dtl_uri);
            
            const std::string load_error_msg () const;

        }; // end of class 'Initializer'
            
        inline const std::string Initializer::load_error_msg () const
        {
            return load_error_msg_;
        }
    
    } // end of namespace 'icu'

} // end of namespace 'cc'

#endif // NRS_CC_ICU_INITIALIZER_H_
