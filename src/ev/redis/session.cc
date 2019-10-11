/**
 * @file json_api.cc - REDIS
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

#include "ev/redis/session.h"

#include "ev/redis/request.h"
#include "ev/redis/error.h"

#include "ev/request.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_iss
 * @param a_token_prefix
 */
ev::redis::Session::Session (const ::ev::Loggable::Data& a_loggable_data,
                             const std::string& a_iss, const std::string& a_token_prefix)
    : iss_(a_iss), token_prefix_(a_token_prefix), loggable_data_(a_loggable_data)
{
    reverse_track_enabled_ = false;
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Copy constructor.
 *
 * @param a_session
 */
ev::redis::Session::Session (const ev::redis::Session& a_session)
    : iss_(a_session.iss_), token_prefix_(a_session.token_prefix_), loggable_data_(a_session.loggable_data_)
{
    data_                  = a_session.data_;
    reverse_track_enabled_ = a_session.reverse_track_enabled_;
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor.
 */
ev::redis::Session::~Session ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
}                                  

/**
 * @brief Set a session data for a token.
 *
 * @param a_data
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::redis::Session::Set (const ev::redis::Session::DataT& a_data,
                              const ev::redis::Session::SuccessCallback a_success_callback, const ev::redis::Session::FailureCallback a_failure_callback)
{
#if 1
    throw ev::Exception("NOT SUPPORTED - TODO REMOVE CALLS TO THIS FUNCTION!");
#else
    const std::string session_key = token_prefix_ + a_data.token_;
    
    auto task = NewTask([this, a_data, session_key] () -> ::ev::Object* {
        
        return new ::ev::redis::Request(loggable_data_, "EXISTS", { session_key });
        
    })->Then([session_key, a_data] (::ev::Object* a_object) -> ::ev::Object* {
                
        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - 1 the key exists
        //  - 0 the key does not exists
        //
        
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 0 != value.Integer() ) {
            throw ev::Exception("Trying to insert a duplicated session id!");
        }
        
        //
        // Store session.
        //
        
        std::vector<std::string> args;
        
        args.push_back(session_key);
        
        for ( auto it : a_data.payload_ ) {
            args.push_back(it.first);
            args.push_back(it.second);
        }
        
        return new ::ev::redis::Request(loggable_data_, "HMSET", args);
        
    })->Then([session_key, a_data] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HMSET:
        //
        // - Status Reply is expected
        //
        //  - 'OK'
        //
        ev::redis::Reply::EnsureIsStatusReply(a_object, "OK");

        //
        // Set expiration.
        //
        return new ::ev::redis::Request(loggable_data_, "EXPIRE", { session_key, std::to_string(a_data.expires_in_) });
        
    });
    
    if ( true == reverse_track_enabled_ ) {

        task->Then([this, a_data] (::ev::Object* a_object) -> ::ev::Object* {

            //
            // EXPIRE:
            //
            // Integer reply, specifically:
            // - 1 if the timeout was set.
            // - 0 if key does not exist or the timeout could not be set.
            //
            ev::redis::Reply::EnsureIntegerReply(a_object, 1);
            
            //
            // Map it.
            //
            // <issuer>/<provider>/<user_id> : list
            return new ::ev::redis::Request(loggable_data_, "SADD", { iss_ + "/" + a_data.provider_ + "/" + a_data.user_id_ , a_data.token_ });

        })->Finally([this, a_data, a_success_callback] (::ev::Object* a_object) {
            
            //
            // SADD:
            //
            // - Integer reply is expected:
            //
            //  -  the number of elements that were added to the set, not including all the elements already present into the set.
            //
            ev::redis::Reply::EnsureIntegerReply(a_object);
            
            //
            // Accept it.
            //
            data_           = a_data;
            data_.verified_ = true;
            data_.exists_   = true;
            
            a_success_callback(data_);
            
        });

    } else {

        task->Finally([this, a_data, a_success_callback] (::ev::Object* a_object) {
            
            //
            // EXPIRE:
            //
            // Integer reply, specifically:
            // - 1 if the timeout was set.
            // - 0 if key does not exist or the timeout could not be set.
            //
            ev::redis::Reply::EnsureIntegerReply(a_object, 1);
            
            //
            // Accept it.
            //
            data_           = a_data;
            data_.verified_ = true;
            data_.exists_   = true;
            
            a_success_callback(data_);
            
        });

    }
    
    task->Catch([this, a_failure_callback] (const ::ev::Exception& a_ev_exception) {
        
        a_failure_callback(data_, a_ev_exception);
        
    });
#endif
}

/**
 * @brief Delete a session data.
 *
 * @param a_token
 * @param a_success_callback
 * @param a_failure_callback
 */
void ev::redis::Session::Unset (const ev::redis::Session::DataT& a_data,
                                const ev::redis::Session::SuccessCallback a_success_callback, const ev::redis::Session::FailureCallback a_failure_callback)
{
#if 1
    throw ev::Exception("NOT SUPPORTED - TODO REMOVE CALLS TO THIS FUNCTION!");
#else
    
    ::ev::scheduler::Task* task;
    
    if ( true == reverse_track_enabled_ ) {
        
        // 0) mapping <issuer>/<provider>/<user_id>
        const std::string mapping_key = iss_ + "/" + a_data.provider_ + "/" + a_data.user_id_;
        
        // ... remove specific token?
        if ( a_data.token_.length() > 0 ) {
            
            //
            // ERASE ONE TOKEN ( <prefix>:<token> ) FOR THIS USER ( mapping key )
            //
            
            //
            // 1) LREM mapping token
            // 2) DEL <prefix>:<token>
            // 3) EXEC
            // f) Check
            //
            
            task = NewTask([this, mapping_key, a_data] () -> ::ev::Object* {
                //
                // DELETE session key stored @ mapping key list
                //
                return new ::ev::redis::Request(loggable_data_, "SREM", { mapping_key, a_data.token_ });
            })->Then([this, a_data](::ev::Object* a_object) -> ::ev::Object* {
                
                //
                // SREM:
                //
                // - Integer reply is expected:
                //
                //  - the number of members that were removed from the set, not including non existing members.
                //
                ev::redis::Reply::EnsureIntegerValueIsGT(ev::redis::Reply::EnsureIntegerReply(a_object), -1);
                
                //
                // DELETE session key
                //
                
                return new ::ev::redis::Request(loggable_data_, "DEL", { token_prefix_ + a_data.token_ });
                
            });
            
        } else {
            
            //
            // ERASE ALL TOKENS ( <prefix>:<token> ) FOR THIS USER ( mapping key )
            //
            
            std::vector<std::string> session_tokens;
            
            //
            // 1) SMEMBERS - get all tokens @ mapping_key set.
            // 2) SREM <mapping_key> <prefix:<token>>, ...
            // 3) DEL <prefix>:<token>, ...
            // f) Check
            //
            
            task = NewTask([mapping_key] () -> ::ev::Object* {
                
                //
                // Get all members stored at mapping key.
                //
                return new ::ev::redis::Request(loggable_data_, "SMEMBERS", { mapping_key });
                
            })->Then([&session_tokens, mapping_key](::ev::Object* a_object) -> ::ev::Object* {
                
                //
                // SMEMBERS:
                //
                // - Array reply is expected:
                //
                //  - all elements of the set.
                //
                const ::ev::redis::Value& value = ev::redis::Reply::EnsureArrayReply(a_object);
                for ( int idx = 0 ; idx < static_cast<int>(value.Size()) ; ++idx ) {
                    session_tokens.push_back(value[idx].String());
                }
                
                //
                // SREM all session tokens stored @ mapping key.
                //
                std::vector<std::string> args;
                args.push_back(mapping_key);
                args.insert(args.begin() + 1, session_tokens.begin(), session_tokens.end());
                
                return new ::ev::redis::Request(loggable_data_, "SREM", args);
                
            })->Then([this, mapping_key, &session_tokens](::ev::Object* a_object) -> ::ev::Object* {
                
                //
                // SREM:
                //
                // - Integer reply is expected:
                //
                //  - the number of members that were removed from the set, not including non existing members.
                //
                //
                
                ev::redis::Reply::EnsureIntegerValueIsGT(ev::redis::Reply::EnsureIntegerReply(a_object), -1);
                
                //
                // DEL session keys
                //
                for ( size_t idx = 0 ; idx < session_tokens.size() ; ++idx ) {
                    session_tokens[idx] = token_prefix_ + session_tokens[idx];
                }
                
                return new ::ev::redis::Request(loggable_data_, "DEL", { session_tokens });
                
            });
            
        }
    } else {
        
        task = NewTask([this, a_data] () -> ::ev::Object* {
            
            return new ::ev::redis::Request(loggable_data_, "DEL", { token_prefix_ + a_data.token_ });
            
        });
        
    }
    
    // ... common action(s) ...
    task->Finally([this, a_success_callback] (::ev::Object* a_object) {
        
        //
        // DEL:
        //
        // - Integer reply is expected:
        //
        //  - the number of keys that were removed.
        //
        ev::redis::Reply::EnsureIntegerValueIsGT(ev::redis::Reply::EnsureIntegerReply(a_object), -1);
        
        //
        // Reset it.
        //
        data_.provider_       = "";
        data_.user_id_        = "";
        data_.token_          = "";
        data_.token_is_valid_ = false;
        data_.payload_.clear();
        data_.expires_in_     = -1;
        data_.verified_       = true;
        data_.exists_         = false;
        
        // notify
        a_success_callback(data_);
        
    })->Catch([this, a_failure_callback] (const ::ev::Exception& a_ev_exception) {
        
        a_failure_callback(data_, a_ev_exception);
        
    });
#endif
}

/**
 * @brief Retrieve the session data for a token.
 *
 * @param a_success_callback
 * @param a_invalid_session_callback
 * @param a_failure_callback
 */
void ev::redis::Session::Fetch (const ev::redis::Session::SuccessCallback a_success_callback,
                                const ev::redis::Session::InvalidCallback a_invalid_session_callback,
                                const ev::redis::Session::FailureCallback a_failure_callback)
{
    data_.verified_ = false;
    data_.exists_   = false;
    
    NewTask([this] () -> ::ev::Object* {
        
        return new ::ev::redis::Request(loggable_data_, "EXISTS", { token_prefix_ + data_.token_ } );
        
    })->Then([this] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - 1 the key exists
        //  - 0 the key does not exists
        //
        
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 1 != value.Integer() ) {
            data_.verified_ = true;
            data_.exists_   = false;
            throw ev::Exception("Session does not exists!");
        }
        
        return new ::ev::redis::Request(loggable_data_, "HGETALL", { token_prefix_ + data_.token_ });
        
    })->Finally([a_success_callback, a_invalid_session_callback, this] (::ev::Object* a_object) {
        
        const ::ev::Result* result = dynamic_cast<const ::ev::Result*>(a_object);
        if ( nullptr == result ) {
            throw ::ev::Exception("Unexpected data object type - expecting " UINT8_FMT " got " UINT8_FMT " !",
				  static_cast<uint8_t>(a_object->type_), static_cast<uint8_t>(::ev::Object::Type::Result)
	    );
        }
        
        const ::ev::Object* data_object = result->DataObject();
        if ( nullptr == data_object ) {
            throw ::ev::Exception("Unexpected data object - nullptr!");
        }
        
        const ::ev::redis::Reply* reply = dynamic_cast<const ::ev::redis::Reply*>(data_object);
        if ( nullptr == reply ) {
            throw ::ev::Exception("Unexpected reply object - nullptr!");
        }
        
        const ::ev::redis::Value& value = reply->value();
        
        switch (value.content_type()) {
            case ::ev::redis::Value::ContentType::Array:
                data_.token_is_valid_ = value.Size() > 0;
                value.IterateHash([this](const ::ev::redis::Value* a_key, const ::ev::redis::Value* a_value){
                    data_.payload_[a_key->String()] = a_value->String();
                });
                break;
            case ::ev::redis::Value::ContentType::Integer:
                throw ::ev::Exception("Logic error: expecting an hash got an integer!");
            case ::ev::redis::Value::ContentType::String:
                throw ::ev::Exception("Logic error: expecting an hash got a string!");
            case ::ev::redis::Value::ContentType::Status:
                throw ::ev::Exception("Logic error: expecting an hash got a status!");
            case ::ev::redis::Value::ContentType::Nil:
                throw ::ev::Exception("Logic error: expecting an hash got nil!");
            default:
                break;
        }

        data_.verified_ = true;
        
        if ( true == data_.token_is_valid_ ) {
            data_.exists_ = true;
            a_success_callback(data_);
        } else {
            data_.exists_ = false;
            a_invalid_session_callback(data_);
        }

    })->Catch([this, a_failure_callback, a_invalid_session_callback] (const ::ev::Exception& a_ev_exception) {
                
        if ( true == data_.verified_ && false == data_.exists_ ) {
            a_invalid_session_callback(data_);
        } else {
            a_failure_callback(data_, a_ev_exception);
        }
        
    });
}

/**
 * @brief Extends the currenlty set session.
 *
 * @param a_amount
 * @param a_success_callback
 * @param a_invalid_session_callback
 * @param a_failure_callback
 */
void ev::redis::Session::Extend (const size_t a_amount,
                                 const ev::redis::Session::SuccessCallback a_success_callback,
                                 const ev::redis::Session::InvalidCallback a_invalid_session_callback,
                                 const ev::redis::Session::FailureCallback a_failure_callback)
{
    bool session_exists = false;

    NewTask([this] () -> ::ev::Object* {
        
        return new ::ev::redis::Request(loggable_data_, "EXISTS", { token_prefix_ + data_.token_ } );
        
    })->Then([this, a_amount, &session_exists] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // EXISTS:
        //
        // - Integer reply is expected:
        //
        //  - 1 the key exists
        //  - 0 the key does not exists
        //
        
        const ev::redis::Value& value = ev::redis::Reply::EnsureIntegerReply(a_object);
        if ( 1 != value.Integer() ) {
            throw ev::Exception("Session does not exists!");
        }

        session_exists = true;

        return new ::ev::redis::Request(loggable_data_, "EXPIRE", { token_prefix_ + data_.token_, std::to_string(a_amount) });
        
    })->Finally([a_success_callback, a_invalid_session_callback, this] (::ev::Object* a_object) {
        
        //
        // EXPIRE:
        //
        // Integer reply, specifically:
        // - 1 if the timeout was set.
        // - 0 if key does not exist or the timeout could not be set.
        //
        ev::redis::Reply::EnsureIntegerReply(a_object, 1);
        
        a_success_callback(data_);

    })->Catch([this, a_failure_callback, a_invalid_session_callback, &session_exists] (const ::ev::Exception& a_ev_exception) {
                
        if ( false == session_exists ) {
            a_invalid_session_callback(data_);
        } else {
            a_failure_callback(data_, a_ev_exception);
        }
        
    });
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @return  A random generated string ( max length 36 ).
 */
std::string ev::redis::Session::Random (uint8_t a_length)
{
    static const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    std::stringstream ss;
    for (int idx = 0; idx < std::min(a_length, (uint8_t)63); ++idx ) {
        ss << alphanum[random() % 62];
    }
    
    return ss.str();
}

/**
 * @brief Check if a random string value alphabet is as expected ( max length 36 ).
 *
 * @return True if the value respects length and session alphabet, false otherwise.
 */
bool ev::redis::Session::IsRandomValid (const std::string& a_value, uint8_t a_length)
{
    if ( static_cast<size_t>(a_length) != a_value.length() ) {
        return false;
    }
    for ( auto idx = 0 ; idx < a_length ; ++idx ) {
        if ( not ( ( a_value[idx] >= 'a' && a_value[idx] <= 'z' ) || ( a_value[idx] >= 'A' && a_value[idx] <= 'Z' ) || ( a_value[idx] >= '0' && a_value[idx] <= '9' ) ) ) {
            return false;
        }
    }
    return true;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* ev::redis::Session::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}
