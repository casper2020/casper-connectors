/**
 * @file string_helper.h
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

#ifndef CC_UTF8_STRING_HELPER_H_
#define CC_UTF8_STRING_HELPER_H_
#pragma once

#include <string>
#include <vector>

namespace cc
{
    
    namespace utf8
    {
        
        /**
         * @brief A UTF8 string helper.
         */
        class StringHelper
        {

        public: // Static Method(s) / Function(s)

            /**
             * @brief A helper function to check if a string starts with another string.
             *
             * @param a_first  Initial string.
             * @param a_second Prefix string
             */
            static bool StartsWith (const std::string& a_first, const std::string& a_second);

            /**
             * @brief A helper function to check if a string contains another string.
             *
             * @param a_first  Initial string.
             * @param a_second The other string
             */
            static bool Contains (const std::string& a_first, const std::string& a_second);

            /**
             * @brief A helper a text should be rejected for a specific filter.
             *
             * @param a_string String to be tested.
             * @param a_filter Filter to be applied.
             * @param o_words  String words.
             */
            static bool FilterOut  (const std::string& a_string, const std::string& a_filter, std::vector<std::string>* o_words);

            /**
             * @brief A helper a split a string in to words.
             *
             * @param a_string String to be splitted.
             * @param o_words  String words.
             */
            static void SplitWords (const std::string& a_string, std::vector<std::string>& o_words);

            /**
             * @brief A helper a replace a string.
             *
             * @param a_string The string to be processed.
             * @param a_from   The string to replace.
             * @param a_to     The new string.
             * @param o_string The final result, with all matches replaced
             */
            static void Replace (const std::string& a_string, const std::string& a_from, const std::string & a_to, std::string& o_string);

            /**
             * @brief Search and replace all occurrences of a string.
             *
             * @param a_source
             * @param a_search
             * @param a_replace
             *
             * @return
             */
            static std::string ReplaceAll (const std::string& a_source, const std::string& a_search, const std::string& a_replace);

            /**
             * @brief A helper function to remove white spaces from the right and left of a string.
             *
             * @param a_string String to be processed
             *
             * @return The final result, with the white spaces removed from left and right.
             */
            static std::string& Trim (std::string& a_string);

            /**
             * @brief A helper function to remove white spaces from the right and left of a string.
             *
             * @param a_string String to be processed
             *
             * @return The final result, with the white spaces removed from left and right.
             */
            static std::string Trim (const char* const a_string);

            /**
             * @brief A helper function to 'collate' a string.
             *
             * @param a_string String to be processed
             *
             * @return The final result.
             */
            static std::string Collate (const char* const a_string);

            /**
             * @brief A helper to encode an URI.
             *
             * @param a_string The URI string.
             * @param o_string The encoded URI string.
             *
             * @throw An \link osal::Exception \link on error.
             */
            static void URIEncode (const std::string& a_string, std::string& o_string);
            
            /**
             * @brief A helper to encode an URI params.
             *
             * @param a_uri The URI string.
             *
             * @return The encoded URI.
             *
             * @throw An \link osal::Exception \link on error.
             */
            static std::string URIEncode (const std::string& a_uri);

            /**
             * @brief A helper to decode an URI.
             *
             * @param a_string The encoded URI string.
             * @param o_string The decoded URI string.
             *
             * @throw An \link osal::Exception \link on error.
             */
            static void URIDecode (const std::string& a_string, std::string& o_string);

            /**
             * @brief Count the number of UTF8 chars and bytes in a string.
             *
             * @param a_string
             *
             * @return The number of UTF8 chars and bytes in a string.
             */
            static std::tuple<size_t, size_t> CountForJson (const std::string& a_string);


            static void JSONEncode (const std::string& a_string, uint8_t* o_out);
            
            static std::string JSONEncode (const std::string& a_string);

        }; // end of class StringHelper
        
    } // end of namespace 'utf8'

} // end of namespace 'cc'

#endif // CC_UTF8_STRING_HELPER_H_
