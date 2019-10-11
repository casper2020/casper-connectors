/**
 * @file errors.cc
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
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/errors/jsonapi/tracker.h"

#include "cc/i18n/singleton.h"

/**
 * @brief Default constructor.
 *
 * @param a_locale
 * @param a_content_type
 * @param a_generic_error_message_key
 * @param a_generic_error_message_with_code_key
 * @param a_enable_nice_piece_of_code_a_k_a_ordered_json
 */
cc::errors::jsonapi::Tracker::Tracker (const std::string& a_locale, const std::string& a_content_type,
                                       const char* const a_generic_error_message_key, const char* const a_generic_error_message_with_code_key,
                                       bool a_enable_nice_piece_of_code_a_k_a_ordered_json)
    : cc::errors::Tracker(a_content_type, a_locale, a_generic_error_message_key, a_generic_error_message_with_code_key),
      enable_nice_piece_of_code_a_k_a_ordered_json_(a_enable_nice_piece_of_code_a_k_a_ordered_json)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::errors::jsonapi::Tracker::~Tracker ()
{
    /* empty */
}

/**
 * @brief Track an error code.
 *
 * @param a_error_code
 * @param a_http_status_code
 * @param a_i18n_key
 */
void cc::errors::jsonapi::Tracker::Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                          const char* const a_i18n_key)
{
    Track(a_error_code, a_http_status_code, a_i18n_key, "", nullptr);
}

/**
 * @brief Track an error code with an internal error message.
 *
 * @param a_error_code
 * @param a_http_status_code
 * @param a_i18n_key
 * @param a_internal_error_message.
 */
void cc::errors::jsonapi::Tracker::Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                          const char* const a_i18n_key, const char* const a_internal_error_msg)
{
    const Json::Value entry = cc::i18n::Singleton::GetInstance().Get(locale_, a_i18n_key);
    
    Track(a_error_code, a_http_status_code, a_i18n_key, "", a_internal_error_msg);
}

/**
 * @brief Track an error code with 'replaceable' arguments.
 *
 * @param a_error_code
 * @param a_http_status_code
 * @param a_i18n_key
 * @param a_args
 * @param a_args_count
 */
void cc::errors::jsonapi::Tracker::Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                          const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count)
{
    Track(a_error_code, a_http_status_code, a_i18n_key, a_args, a_args_count, nullptr);
}

/**
 * @brief Track an error code with 'replaceable' arguments and an internal error message.
 *
 * @param a_error_code
 * @param a_http_status_code
 * @param a_i18n_key
 * @param a_args
 * @param a_args_count
 * @param a_internal_error_msg
 */
void cc::errors::jsonapi::Tracker::Track (const char* const a_error_code, const uint16_t a_http_status_code,
                                          const char* const a_i18n_key, U_ICU_NAMESPACE::Formattable* a_args, size_t a_args_count,
                                          const char* const a_internal_error_msg)
{
    const Json::Value entry = cc::i18n::Singleton::GetInstance().Get(locale_, a_i18n_key);
    
    std::string what;
    
    if ( false == entry.isNull() ) {
        
        UErrorCode error = U_ZERO_ERROR;
        
        U_ICU_NAMESPACE::UnicodeString result;
        
        U_ICU_NAMESPACE::MessageFormat::format(entry.asString().c_str(),
                                               a_args, static_cast<int32_t>(a_args_count),
                                               result, error
                                               );
        
        if ( U_ZERO_ERROR == error ) {
            result.toUTF8String(what);
        } else {
            what = a_i18n_key;
        }
        
    } else {
        what = entry.asString();
    }
    
    Track(a_error_code, a_http_status_code, a_i18n_key, what, a_internal_error_msg);
}

/**
 * @brief Transform collected errors to a JSON object.
 *
 * @param o_object
 */
void cc::errors::jsonapi::Tracker::jsonify (Json::Value& o_object)
{
    o_object = array_;
}

/**
 * @brief Serialize collected errors in to a JSON string.
 *
 * @return A string in JSON format with all collected errors.
 */
std::string cc::errors::jsonapi::Tracker::Serialize2JSON () const
{
    Json::FastWriter writer;
    writer.omitEndingLineFeed();

    if ( true == enable_nice_piece_of_code_a_k_a_ordered_json_ ) {
        
        // ... ensuring 'brilliant' idea of 'ordered' JSON serialization ...
        //    {
        //        "errors": [
        //                      {
        //                          "status": nullptr,
        //                          "code": nullptr,
        //                          "detail": nullptr,
        //                          "meta": {
        //                              "internal-error": nullptr
        //                           }
        //                      }
        //         ]
        //    }
        std::stringstream ss;
        
        ss << "{\"errors\":[";
        
        for ( Json::ArrayIndex idx = 0 ; idx < array_.size(); ++idx ) {
            
            const Json::Value& error = array_[idx];
            
            ss << "{\"status\":"  << "\"" << error["status"].asString() << "\"," << "\"code\":\"" << error["code"].asString() << "\"";
            if ( true == error.isMember("detail") ) {
                ss << ",\"detail\":" << writer.write(error["detail"]);
            }
            ss << '}';
        }
        
        ss << "]}";
        
        return ss.str();
        
    } else {
        Json::Value      errors_object = Json::Value(Json::ValueType::objectValue);
        
        errors_object["errors"] = array_;
                
        return writer.write(errors_object);
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Keep track of a non-exception error for a specific connection.
 *
 * @param a_error_code
 * @param a_http_status_code
 * @param a_i18n_key
 * @param a_detail
 * @param a_internal
 */
void cc::errors::jsonapi::Tracker::Track (const char* const /* a_error_code */, const uint16_t a_http_status_code,
                                          const std::string& a_i18n_key, const std::string& a_detail,
                                          const char* const a_internal)
{
    //    {
    //        "errors": [
    //                      {
    //                          "status": nullptr,
    //                          "code": nullptr,
    //                          "detail": nullptr,
    //                          "meta": {
    //                              "internal-error": nullptr
    //                           }
    //                      }
    //         ]
    //    }
    
    Json::Value ev_error_object = Json::Value(Json::ValueType::objectValue);
    
    // ... http status code ...
    const auto status_code_it = cc::i18n::Singleton::k_http_status_codes_map_.find(a_http_status_code);
    if ( cc::i18n::Singleton::k_http_status_codes_map_.end() != status_code_it ) {
        ev_error_object["status"] = std::to_string(a_http_status_code) + " - " + status_code_it->second;
    } else {
        ev_error_object["status"] = std::to_string(a_http_status_code);
    }
    
    // ... application specific error code ...
    ev_error_object["code"] = a_i18n_key;
    if ( a_detail.length() > 0 ) {
        ev_error_object["detail"] = a_detail;
    } else {
        
        std::string error_detail;
        
        
        const bool msg_with_error_code = ( a_i18n_key.length() > 0 && generic_error_message_with_code_key_.length() > 0 );
        
        const Json::Value entry = cc::i18n::Singleton::GetInstance().Get(
                                                                         locale_,
                                                                         msg_with_error_code ? generic_error_message_with_code_key_.c_str() : generic_error_message_key_.c_str()
                                                                        );
        if ( false == entry.isNull() ) {
            
            UErrorCode                     error = U_ZERO_ERROR;
            U_ICU_NAMESPACE::UnicodeString result;
            
            if ( true == msg_with_error_code ) {
                const U_ICU_NAMESPACE::Formattable args[] = {
                    a_i18n_key.c_str(),
                };
                U_ICU_NAMESPACE::MessageFormat::format(entry.asString().c_str(),
                                                       args, 1,
                                                       result, error
                                                       );
            } else {
                const U_ICU_NAMESPACE::Formattable args[] = {
                };
                U_ICU_NAMESPACE::MessageFormat::format(entry.asString().c_str(),
                                                       args, 0,
                                                       result, error
                                                       );
            }
            
            if ( U_ZERO_ERROR == error ) {
                result.toUTF8String(error_detail);
            }
        }
        
        if ( error_detail.length() > 0 ) {
            ev_error_object["detail"] = error_detail;
        } else {
            if ( true == msg_with_error_code ) {
                ev_error_object["detail"] = "Error " + a_i18n_key + " occurred while processing your request. Please contact technical support.";
            } else {
                ev_error_object["detail"] = "An error occurred while processing your request. Please contact technical support.";
            }
        }
    }
    
    // ... non standard information about the error ...
    if ( nullptr != a_internal ) {
        ev_error_object["meta"]["internal-error"] = a_internal;
    }
    
    // ... keep track of it ...
    array_.append(ev_error_object);
}
