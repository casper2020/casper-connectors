/**
* @file redis.cc
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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

#include "cc/easy/redis.h"

#include "cc/types.h"

#include "cc/exception.h"

/**
 *
 * @brief Default constructor.
 */
cc::easy::Redis::Redis ()
 : context_(nullptr), last_reply_(nullptr)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::easy::Redis::~Redis ()
{
    Disconnect();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Connect to a REDIS instance.
 *
 * @param a_ip   Host ip address.
 * @param a_port Host port number.
 */
void cc::easy::Redis::Connect (const std::string& a_ip, const int a_port)
{
    // ... already connected?
    if ( nullptr == context_ ) {
        Disconnect();
    }
    // ... make a new connection ...
    context_ = redisConnect(a_ip.c_str(), a_port);
    if ( nullptr == context_ ) {
        throw ::cc::Exception("Unable to connect to redis %s:%d - could't create context!", a_ip.c_str(), a_port);
    }
    // ... if got an error?
    if ( 0 != context_->err ) {
        // ... keep treack of message ...
        const std::string msg = context_->errstr;
        // ... free context ...
        redisFree(context_);
        context_ = nullptr;
        // ... notify caller ...
        throw ::cc::Exception("Unable to connect to redis %s:%d - %s!", a_ip.c_str(), a_port, msg.c_str());
    }
}

/**
 * @brief Disconnect from a REDIS connection.
 */
void cc::easy::Redis::Disconnect ()
{
    // ... already disconnected?
    if ( nullptr == context_ ) {
        return;
    }
    // ... free context ...
    redisFree(context_);
    context_ = nullptr;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Build and execute INCR command.
 *
 * @param a_key Key to set expiration value.
 *
 * @return Value after increment.
 */
long long cc::easy::Redis::INCR (const std::string& a_key)
{
    //
    // INCR:
    //
    // - An integer reply is expected:
    //
    //  - the value of key after the increment
    //
    const ::ev::redis::Reply  reply = ExecuteCommand("INCR %s", a_key.c_str());
    const ::ev::redis::Value& value = ::ev::redis::Reply::EnsureIntegerReply(reply);
    
    return value.Integer();
}

/**
 * @brief Build and execute HSET command.
 *
 * @param a_key   Key to set expiration value.
 * @param a_field Field name.
 * @param a_value Field value.
 */
void cc::easy::Redis::HSET (const std::string& a_key, const std::string& a_field, const std::string& a_value)
{
    //
    // HSET:
    //
    // - An integer reply is expected:
    //
    //  - 1 if field is a new field in the hash and value was set.
    //  - 0 if field already exists in the hash and the value was updated.
    //
    const ::ev::redis::Reply& reply = ExecuteCommand("HSET %s %s %s", a_key.c_str(), a_field.c_str(), a_value.c_str());
    (void)::ev::redis::Reply::EnsureIntegerReply(reply);
}


/**
 * @brief Build and execute EXPIRE command.
 *
 * @param a_key     Key to set expiration value.
 * @param a_seconds Key validity in seconds.
*/
void cc::easy::Redis::EXPIRE (const std::string& a_key, const size_t& a_seconds)
{
    //
    // EXPIRE:
    //
    // Integer reply, specifically:
    // - 1 if the timeout was set.
    // - 0 if key does not exist or the timeout could not be set.
    //
    const ::ev::redis::Reply& reply = ExecuteCommand("EXPIRE %s " UINT64_FMT, a_key.c_str(), static_cast<uint64_t>(a_seconds));
    const ::ev::redis::Value& value = ::ev::redis::Reply::EnsureIntegerReply(reply);
    if ( 0 == value.Integer() ) {
        throw ::cc::Exception("Could't set key %s expiration - does not exist or the timeout could not be set", a_key.c_str());
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Ensure we've a valid connection to REDIS.
 *
 * @param a_action The action that request this validation.
 */
void cc::easy::Redis::EnsureConnection (const char* const a_action)
{
    if ( nullptr == context_ ) {
        throw ::cc::Exception("Could't %s - no connection established.", a_action);
    }
}

/**
 * @brief Build and execute a REDIS command.
 *
 * @param a_format Command value format.
 * @param ...      Command value arguments.
 *
 * @return Pointer to last executed command reply.
 */
const ::ev::redis::Reply& cc::easy::Redis::ExecuteCommand (const char* const a_format, ...)
{
    // ... ensure a valid connection is already established ...
    EnsureConnection("execute command");
    // ... capture arguments ...
    va_list  args;
    va_start(args, a_format);
    // ... make request ...
    redisReply* reply = static_cast<redisReply*>(redisvCommand(context_, a_format, args));
    va_end(args);
    assert(reply != NULL);
    // ... copy reply value ...
    last_reply_ = reply;
    freeReplyObject(reply);
    // ... we're doen ...
    return last_reply_;
}

