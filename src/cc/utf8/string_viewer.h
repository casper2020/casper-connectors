/**
 * @file string_viewer.h
 *
 * Based on code originally developed for NDrive S.A.
 *
 * Copyright (c) 2011-2023 Cloudware S.A. All rights reserved.
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
 * along with osal.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CC_UTF8_STRING_VIEWER_H_
#define CC_UTF8_STRING_VIEWER_H_
#pragma once

#include <string> // std::string
#include <vector> // std::vector
#include <map>    // std::map
#include <tuple>  // std::tuple
#include <string.h> // strcmp

#include "cc/exception.h"

namespace cc
{
    
    namespace utf8
    {
        
        class StringViewer
        {

        private: // Data

            const char*         start_ptr_;
            const char*         current_ptr_;
            uint16_t            current_char_;

        public: // Constructor(s) / Destructor

            StringViewer (const char* a_string);
            StringViewer (const std::string& a_string);
            virtual ~StringViewer ();

        public: // Inline Method(s) / Function(s)

            inline uint16_t Current   () const;
            inline size_t   Remaining () const;
            inline uint16_t Next      ();

        public: // Overloaded Operator(s)

            inline StringViewer& operator ++ ();
            inline StringViewer& operator += (int);
            inline StringViewer& operator -- ();
            inline StringViewer& operator -= (int);

            inline StringViewer& operator  = (const char*);
            inline StringViewer& operator  = (const StringViewer&);

            inline bool operator == (const char*);
            inline bool operator == (const StringViewer&);

        public: // Static Method(s) / Function(s)

            static size_t                       CharsCount   (const std::string& a_string);
            static std::tuple<size_t, size_t>   Count        (const std::string& a_string);

        }; // end of class UTF8Iteraror

        /**
         * @brief Return the current UTF8 char.
         */
        inline uint16_t StringViewer::Current() const
        {
            return current_char_;
        }

        /**
         * @return The number of remaining UTF8 chars.
         */
        inline size_t StringViewer::Remaining () const
        {
            const char* start_ptr    = start_ptr_;
            const char* current_ptr  = current_ptr_;
            size_t      count        = 0;
            uint16_t    current_char = 0;

            if ( '\0' == (*start_ptr) ) {
                return 0;
            }

            do {
                current_char = static_cast<uint16_t>(*(current_ptr++));
                if ( current_char >= 0x80 ) {
                    if ( (current_char & 0xE0) == 0xC0 ) {
                        current_char &= 0x1F;
                        current_char <<= 6;
                        current_char |= *(current_ptr++) & 0x3F;
                    } else {
                        current_char &= 0x0F;
                        current_char <<= 6;
                        current_char |= *(current_ptr++) & 0x3F;
                        current_char <<= 6;
                        current_char |= *(current_ptr++) & 0x3F;
                    }
                }
                count++;
            } while ( 0 != current_char );

            return count - 1; // -1 ~> \0a
        }

        /**
         * @brief Overloaded '++' operator.
         */
        inline StringViewer& StringViewer::operator ++ ()
        {
            throw cc::Exception("%s", "Unsupported operator++!");
        }

        /**
         * @brief Overloaded '+=' operator.
         */
        inline StringViewer& StringViewer::operator += (int)
        {
            throw cc::Exception("%s", "Unsupported operator+=!");
        }

        /**
         * @brief Overloaded '--' operator.
         */
        inline StringViewer& StringViewer::operator -- ()
        {
            throw cc::Exception("%s", "Unsupported operator--!");
        }

        /**
         * @brief Overloaded '+=' operator.
         */
        inline StringViewer& StringViewer::operator -= (int)
        {
            throw cc::Exception("%s", "Unsupported operator-=!");
        }

        /**
         * @brief Overloaded '=' operator.
         */
        inline StringViewer& StringViewer::operator = (const char* a_string)
        {
            start_ptr_    = a_string;
            current_ptr_  = start_ptr_;
            current_char_ = 0;
            return *this;
        }

        /**
         * @brief Overloaded '=' operator.
         */
        inline StringViewer& StringViewer::operator = (const StringViewer& a_iterator)
        {
            start_ptr_    = a_iterator.start_ptr_;
            current_ptr_  = a_iterator.current_ptr_;
            current_char_ = a_iterator.current_char_;
            return *this;
        }

        /**
         * @brief Overloaded '==' operator.
         */
        inline bool StringViewer::operator == (const char* a_string)
        {
            if ( nullptr == a_string || nullptr == start_ptr_ ) { // both null
                return start_ptr_ == a_string;
            }
            // same content?
            return 0 == strcmp(start_ptr_, a_string);
        }

        /**
         * @brief Overloaded '==' operator.
         */
        inline bool StringViewer::operator == (const StringViewer& a_iterator)
        {
            // same content?
            return ( a_iterator.start_ptr_ == start_ptr_ && a_iterator.current_ptr_ == current_ptr_ );
        }

        /**
         * @brief Advance to next char.
         *
         * @return The current char value.
         */
        inline uint16_t StringViewer::Next ()
        {
            current_char_ = static_cast<uint16_t>(*(current_ptr_++));
            if ( current_char_ >= 0x80 ) {
                if ( (current_char_ & 0xE0) == 0xC0 ) {
                    current_char_ &= 0x1F;
                    current_char_ <<= 6;
                    current_char_ |= *(current_ptr_++) & 0x3F;
                } else {
                    current_char_ &= 0x0F;
                    current_char_ <<= 6;
                    current_char_ |= *(current_ptr_++) & 0x3F;
                    current_char_ <<= 6;
                    current_char_ |= *(current_ptr_++) & 0x3F;
                }
            }
            return current_char_;
        }
        
    } // end of namespace 'utf8'

} // end of namespace 'cc'

#endif // CC_UTF8_STRING_VIEWER_H_
