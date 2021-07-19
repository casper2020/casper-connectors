/**
 * @file reply.cc - REDIS Reply
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

#include "ev/redis/reply.h"

#include "ev/redis/error.h"
#include "ev/result.h"

#include "osal/osalite.h"

#include <strings.h> // strcasecmp

/**
 * @brief Default constructor.
 */
ev::redis::Reply::Reply (const struct redisReply* a_reply)
    : ev::redis::Object(ev::redis::Object::Type::Reply)
{
    value_ = a_reply;
}

/**
 * @brief Copy constructor.
*/
ev::redis::Reply::Reply(const ev::redis::Reply& a_reply)
 : ev::redis::Object(a_reply)
{
    value_ = a_reply.value_;
}

/**
 * @brief Destructor.
 */
ev::redis::Reply::~Reply ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Ensure object is a result object and its data object is a reply object.
 *
 * @param a_object
 *
 * @return The command reply value object.
 */
const ev::redis::Value& ev::redis::Reply::GetCommandReplyValue (const ::ev::Object* a_object)
{
    const ::ev::Result* result = dynamic_cast<const ::ev::Result*>(a_object);
    if ( nullptr == result ) {
        throw ::ev::Exception("Unexpected result object type - expecting " UINT8_FMT " got nullptr!",
                              static_cast<uint8_t>(::ev::Object::Type::Result)
        );
    }
    
    const ::ev::Object* data_object = result->DataObject();
    if ( nullptr == data_object ) {
        throw ::ev::Exception("Unexpected data object - nullptr!");
    }
    
    const ::ev::redis::Error* error = dynamic_cast<const ::ev::redis::Error*>(data_object);
    if ( nullptr != error ) {
        throw ::ev::Exception(error->message());
    }
    
    const ::ev::redis::Reply* reply = dynamic_cast<const ::ev::redis::Reply*>(data_object);
    if ( nullptr == reply ) {
        throw ::ev::Exception("Unexpected data object type - expecting " UINT8_FMT " got " UINT8_FMT "!",
                              static_cast<uint8_t>(::ev::Object::Type::Reply), static_cast<uint8_t>(data_object->type_)
        );
    }
    
    return reply->value();
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'status' value.
 *
 * @param a_object
 * @param a_value
 */
void ev::redis::Reply::EnsureIsStatusReply (const ::ev::Object* a_object, const char* const a_value)
{
    EnsureStatusValue(GetCommandReplyValue(a_object), a_value);
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'string' value.
 *
 * @param a_object
 */
const ev::redis::Value& ev::redis::Reply::EnsureStringReply (const ::ev::Object* a_object)
{
    const ::ev::redis::Value& value = GetCommandReplyValue(a_object);
    if ( false == value.IsString() ) {
        if ( true == value.IsError() ) {
            throw ::ev::Exception(value.Error());
        } else {
            throw ::ev::Exception("Unexpected value type - expecting " UINT8_FMT ", got " UINT8_FMT "!",
                                  static_cast<uint8_t>(::ev::redis::Value::ContentType::String), static_cast<uint8_t>(value.content_type())
            );
        }
    }
    return value;
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'integer' value.
 *
 * @param a_object
 */
const ev::redis::Value& ev::redis::Reply::EnsureIntegerReply (const ::ev::Object* a_object)
{
    const ::ev::redis::Value& value = GetCommandReplyValue(a_object);
    if ( false == value.IsInteger() ) {
        if ( true == value.IsError() ) {
            throw ::ev::Exception(value.Error());
        } else {
            throw ::ev::Exception("Unexpected value type - expecting " UINT8_FMT ", got " UINT8_FMT "!",
                                  static_cast<uint8_t>(::ev::redis::Value::ContentType::Integer), static_cast<uint8_t>(value.content_type())
            );
        }
    }
    return value;
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'integer' value.
 *
 * @param a_object
 * @param a_value
 */
void ev::redis::Reply::EnsureIntegerReply (const ::ev::Object* a_object, const long long a_value)
{
    EnsureIntegerValueIsEQ(GetCommandReplyValue(a_object), a_value);
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'integer' value ( greater than ).
 *
 * @param a_object
 * @param a_value
 */
void ev::redis::Reply::EnsureIntegerReplyGT (const ::ev::Object* a_object, const long long a_value)
{
    EnsureIntegerValueIsGT(GetCommandReplyValue(a_object), a_value);
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'array' value.
 *
 * @param a_object
 */
const ::ev::redis::Value& ev::redis::Reply::EnsureArrayReply (const ::ev::Object* a_object)
{
    const ::ev::redis::Value& value = GetCommandReplyValue(a_object);
    if ( false == value.IsArray() ) {
        if ( true == value.IsError() ) {
            throw ::ev::Exception(value.Error());
        } else {
            throw ::ev::Exception("Unexpected value type - expecting " UINT8_FMT", got " UINT8_FMT " !",
                                  static_cast<uint8_t>(::ev::redis::Value::ContentType::Array), static_cast<uint8_t>(value.content_type())
            );
        }
    }
    return value;
}

/**
 * @brief Ensure object is a result object, its data object is a reply object and it contains a valid 'array' value.
 *
 * @param a_object
 * @param a_size
 */
const ::ev::redis::Value& ev::redis::Reply::EnsureArrayReply (const ::ev::Object* a_object, const size_t a_size)
{
    const ::ev::redis::Value& value = EnsureArrayReply(a_object);
    if ( a_size != value.Size() ) {
        throw ::ev::Exception("Unexpected value  - expecting " SIZET_FMT " got " SIZET_FMT " !",
                              a_size, value.Size()
        );
    }
    return value;
}


/**
 * @brief Ensure object is a 'status' object and it has a specific value.
 *
 * @param a_object
 * @param a_value
 */
void ev::redis::Reply::EnsureStatusValue (const ::ev::redis::Value& a_object, const char* const a_value)
{
    if ( false == a_object.IsStatus() ) {
        if ( true == a_object.IsError() ) {
            throw ::ev::Exception(a_object.Error());
        } else {
            throw ::ev::Exception("Unexpected value type - expecting " UINT8_FMT ", got " UINT8_FMT " !",
                                  static_cast<uint8_t>(::ev::redis::Value::ContentType::Status), static_cast<uint8_t>(a_object.content_type())
            );
        }
    }
    if ( 0 != strcasecmp(a_object.Status().c_str(), a_value) ) {
        throw ::ev::Exception("Unexpected value  - expecting '%s' got '%s' !",
                              a_value, a_object.String().c_str()
        );
    }
}

/**
 * @brief Ensure a reply object and it contains a valid 'integer' value.
 *
 * @param a_reply
 * @param a_value
 */
const ev::redis::Value& ev::redis::Reply::EnsureIntegerReply (const ::ev::redis::Reply& a_reply)
{
    const ::ev::redis::Value& value = a_reply.value();
    if ( false == value.IsInteger() ) {
        if ( true == value.IsError() ) {
            throw ::ev::Exception(value.Error());
        } else {
            throw ::ev::Exception("Unexpected value type - expecting " UINT8_FMT ", got " UINT8_FMT "!",
                                  static_cast<uint8_t>(::ev::redis::Value::ContentType::Integer), static_cast<uint8_t>(value.content_type())
            );
        }
    }
    return value;
}

/**
 * @brief Ensure object is a 'integer' object and it's value is equal to the provided one.
 *
 * @param a_object
 * @param a_value
 * @param comparator
 */
void ev::redis::Reply::EnsureIntegerValueIsEQ (const ::ev::redis::Value& a_object, const long long a_value)
{
    EnsureIntegerValue(a_object, a_value, [] (const long long a_lhs, const long long a_rhs) {
        return ( a_lhs == a_rhs );
    });
}

/**
 * @brief Ensure object is a 'integer' object and it's value is greater than the provided one.
 *
 * @param a_object
 * @param a_value
 * @param comparator
 */
void ev::redis::Reply::EnsureIntegerValueIsGT (const ::ev::redis::Value& a_object, const long long a_value)
{
    EnsureIntegerValue(a_object, a_value, [] (const long long a_lhs, const long long a_rhs) {
        return ( a_lhs > a_rhs );
    });
}

/**
 * @brief Ensure object is a 'integer' object and it has a specific value.
 *
 * @param a_object
 * @param a_value
 */
void ev::redis::Reply::EnsureIntegerValue (const ::ev::redis::Value& a_object, const long long a_value)
{
    EnsureIntegerValue(a_object, a_value, [] (const long long a_lhs, const long long a_rhs) {
        return ( a_lhs == a_rhs );
    });
}

/**
 * @brief Ensure object is a 'integer' object and it has a specific value.
 *
 * @param a_object
 * @param a_value
 * @param comparator
 */
void ev::redis::Reply::EnsureIntegerValue (const ::ev::redis::Value& a_object, const long long a_value,
                                             const std::function<bool(const long long a_lhs, const long long a_rhs)> a_comparator)
{
    if ( false == a_object.IsInteger() ) {
        if ( true == a_object.IsError() ) {
            throw ::ev::Exception(a_object.Error());
        } else {
            throw ::ev::Exception("Unexpected value type - expecting " UINT8_FMT ", got " UINT8_FMT " !",
                                  static_cast<uint8_t>(::ev::redis::Value::ContentType::Integer), static_cast<uint8_t>(a_object.content_type())
            );
        }
    }
    if ( false == a_comparator(a_object.Integer(), a_value) ) {
        throw ::ev::Exception("Unexpected value  - expecting " LONG_LONG_FMT " got " LONG_LONG_FMT " !",
                              a_value, a_object.Integer()
        );
    }
}
