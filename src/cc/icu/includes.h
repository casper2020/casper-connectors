/**
* @file initializer.h
*
* Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors..
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
#ifndef NRS_CC_ICU_INCLUDES_H_
#define NRS_CC_ICU_INCLUDES_H_

#include "cc/pragmas.h"

CC_DIAGNOSTIC_PUSH()
CC_DIAGNOSTIC_IGNORED("-Wsign-conversion")

#include "unicode/locid.h" // locale
#include "unicode/ustring.h"
#include "unicode/unistr.h"
#include "unicode/datefmt.h"  // U_ICU_NAMESPACE::DateFormat
#include "unicode/smpdtfmt.h" // U_ICU_NAMESPACE::SimpleDateFormat
#include "unicode/dtptngen.h" // U_ICU_NAMESPACE::DateTimePatternGenerator

#include <cmath> // std::nan

CC_DIAGNOSTIC_POP()

namespace cc {
    
    namespace icu {
     
        template <class E>
        static std::string FormatDate (const U_ICU_NAMESPACE::Locale& a_locale, const UDate& a_value, const char* const a_pattern) {
            UErrorCode error_code = UErrorCode::U_ZERO_ERROR;
            U_ICU_NAMESPACE::DateTimePatternGenerator* generator = U_ICU_NAMESPACE::DateTimePatternGenerator::createInstance(a_locale, error_code);
            if ( U_FAILURE(error_code) ) {
                delete generator;
                throw E("%s", u_errorName(error_code));
            }
            U_ICU_NAMESPACE::UnicodeString pattern = generator->getBestPattern(U_ICU_NAMESPACE::UnicodeString::fromUTF8(a_pattern), error_code);
            if ( U_FAILURE(error_code) ) {
                delete generator;
                throw E("%s", u_errorName(error_code));
            }

            U_ICU_NAMESPACE::SimpleDateFormat* formatter = new U_ICU_NAMESPACE::SimpleDateFormat(pattern, a_locale, error_code);
            if ( U_FAILURE(error_code) ) {
                delete generator;
                delete formatter;
                throw E("%s", u_errorName(error_code));
            }
            
            U_ICU_NAMESPACE::UnicodeString formatted;
            formatter->format((UDate)(a_value), formatted, error_code);
            if ( U_FAILURE(error_code) ) {
                delete generator;
                delete formatter;
                throw E("%s", u_errorName(error_code));
            }
            
            std::string result;
            
            formatted.toUTF8String(result);

            delete generator;
            delete formatter;

            return result;
         
        }
        
        template <class E>
        static UDate ParseDate (const U_ICU_NAMESPACE::Locale& a_locale, const char* a_value, const char* a_pattern)
        {
            UDate      date       = std::nan("");
            UErrorCode error_code = UErrorCode::U_ZERO_ERROR;
            U_ICU_NAMESPACE::SimpleDateFormat* date_format = new U_ICU_NAMESPACE::SimpleDateFormat(U_ICU_NAMESPACE::UnicodeString(a_pattern), a_locale, error_code);
            if (UErrorCode:: U_ZERO_ERROR == error_code || UErrorCode::U_USING_DEFAULT_WARNING == error_code || UErrorCode::U_USING_FALLBACK_WARNING == error_code ) {
                const UDate parsed_date = date_format->parse(U_ICU_NAMESPACE::UnicodeString(a_value), error_code);
                if ( UErrorCode::U_ZERO_ERROR == error_code || UErrorCode::U_USING_DEFAULT_WARNING == error_code || UErrorCode::U_USING_FALLBACK_WARNING == error_code ) {
                    if ( -3600000 == parsed_date ) {
                        date = 0.0f;
                    } else {
                        date = parsed_date;
                    }
                }
            }
            delete date_format;
            return date;
        }
        
    }
}

#endif // NRS_CC_ICU_INCLUDES_H_
