/**
 * @file x509.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_CRT_X509_H_
#define NRS_CC_CRT_X509_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include <string>
#include <vector>

#include "openssl/x509.h"

namespace cc
{
    namespace crt
    {
    
        class X509Reader final : public ::cc::NonCopyable, public ::cc::NonMovable
        {
        
        private: // Data
            
            X509*      x509_;
            X509_NAME* name_ptr_;
            
        public: // Constructor(s) / Destructor
            
            X509Reader ();
            virtual ~X509Reader ();
            
        public: // Method(s) / Function(s)
            
            void Load   (const std::string& a_pem);
            void Unload ();
            
        public: // Method(s) / Function(s)
            
            bool IsCA         () const;
            
            void        GetIssuerDN  (std::string& o_value) const;
            std::string GetIssuerDN  () const;
            void        GetSubjectDN (std::string& o_value) const;
            std::string GetSubjectDN () const;
            
            size_t GetEntry    (const int a_nid, std::string& o_value) const;
            void   GetEntry    (const int a_nid, std::vector<std::string>& o_values) const;
            void   GetValidity (std::string& o_valid_from, std::string& o_valid_to, std::string& o_status) const;
            
            void   Dump (FILE* a_fp);
            
        public: // Static Method(s) / Function(s)
            
            static std::string Fold (const char* const a_pem);
            
        private: // Method(s) / Function(s)
            
            void ANS1_UTF8_STRING (const ASN1_STRING* a_value, std::string& o_value) const;
            
        }; // end of class 'X509Reader'
    
    } // end of namespace 'cc'

} // end of namespace 'cc'

#endif // NRS_CC_CRT_X509_H_
