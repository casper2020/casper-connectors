/**
 * @file session.cc
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ev/casper/session.h"

#include <algorithm> // std::min
#include <sstream> // std::stringstream

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_iss
 * @param a_token_prefix
 */
ev::casper::Session::Session (const ::ev::Loggable::Data& a_loggable_data,
                              const std::string& a_iss, const std::string& a_token_prefix)
    : ev::redis::Session(a_loggable_data, a_iss, a_token_prefix)
{
    /* empty */
}

/**
 * @brief Copy constructor.
 *
 * @param a_session
 */
ev::casper::Session::Session (const ev::casper::Session& a_session)
    : ev::redis::Session(a_session)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::casper::Session::~Session()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Patch a JSON object with session data.
 *
 * @param o_object
 */
void ev::casper::Session::Patch (Json::Value& a_object) const
{
    const std::map<std::string, std::string> patchables = {
        { "user_id"          , GetValue("user_id"          , "") },
        { "company_id"       , GetValue("company_id"       , "") },
        { "company_schema"   , GetValue("company_schema"   , "") },
        { "sharded_schema"   , GetValue("sharded_schema"   , "") },
        { "accounting_schema", GetValue("accounting_schema", "") },
        { "accounting_prefix", GetValue("accounting_prefix", "") },
        { "refresh_token"    , GetValue("refresh_token"    , "") },
        { "access_token"     , data_.token_                      }
    };
    
    Patch("", a_object, patchables);
}

#ifdef __APPLE__
#pragma mark -
#endif
    
/**
 * @brief Recursively patch a JSON object with session data.
 *
 * @param a_name
 * @param a_object
 * @param a_patchables
 */
void ev::casper::Session::Patch (const std::string& a_name, Json::Value& a_object, const std::map<std::string, std::string>& a_patchables) const
{
    switch ( a_object.type() ) {
        case Json::ValueType::objectValue:   // object value (collection of name/value pairs)
            for( auto member : a_object.getMemberNames()) {
                Patch(member, a_object[member], a_patchables);
            }
            break;
        case Json::ValueType::arrayValue:    // array value (ordered list)
            for ( auto ait = a_object.begin(); ait != a_object.end(); ++ait ) {
                Patch("", *ait, a_patchables);
            }
            break;
        case Json::ValueType::stringValue:   // UTF-8 string value
        case Json::ValueType::nullValue:    // 'null' value
        case Json::ValueType::intValue:     // signed integer value
        case Json::ValueType::uintValue:    // unsigned integer value
        case Json::ValueType::realValue:    // double value
        case Json::ValueType::booleanValue: // bool value
        default:
        {
            const auto it = a_patchables.find(a_name);
            if ( a_patchables.end() != it ) {
                a_object = it->second;
            }
            break;
        }
    }
}
