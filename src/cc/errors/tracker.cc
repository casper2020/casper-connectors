/**
 * @file tracker.h
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

#include "cc/errors/tracker.h"

/**
 * @brief Default constructor.
 *
 * @param a_content_Type
 * @param a_locale
 * @param a_generic_error_message_key
 * @param a_generic_error_message_with_code_key
 */
cc::errors::Tracker::Tracker (const std::string& a_content_type, const std::string& a_locale,
                              const char* const a_generic_error_message_key, const char* const a_generic_error_message_with_code_key)
    : content_type_(a_content_type), locale_(a_locale),
      generic_error_message_key_(a_generic_error_message_key),
      generic_error_message_with_code_key_(nullptr != a_generic_error_message_with_code_key ? a_generic_error_message_with_code_key : "")

{
    array_ = Json::Value(Json::ValueType::arrayValue);
}

/**
 * @brief Destructor.
 */
cc::errors::Tracker::~Tracker ()
{
    /* empty */
}
