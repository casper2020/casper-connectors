/**
 * @file reply.h - REDIS Reply
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
#pragma once
#ifndef NRS_EV_REDIS_REPLY_H_
#define NRS_EV_REDIS_REPLY_H_

#include <sys/types.h> // uint8_t, size_t, etc
#include <string>      // std::string

#include "ev/redis/object.h"
#include "ev/redis/value.h"

namespace ev
{
    
    namespace redis
    {
        
        class Reply : public ev::redis::Object
        {
            
        protected:
            
            Value value_; //!< Translate value from a \link redisReply \link.
            
        public: // Constructor(s) / Destructor
            
            Reply(const struct redisReply* a_reply);
            Reply(const Reply& a_reply);
            virtual ~Reply();
            
        public: // Static Method(s) / Function(s)
            
            static const ::ev::redis::Value& GetCommandReplyValue        (const ::ev::Object* a_object);
            
            static void                      EnsureIsStatusReply  (const ::ev::Object* a_object, const char* const a_value);
            
            static const ::ev::redis::Value& EnsureStringReply   (const ::ev::Object* a_object);
                        
            static const ::ev::redis::Value& EnsureIntegerReply   (const ::ev::Object* a_object);
            static void                      EnsureIntegerReply   (const ::ev::Object* a_object, const long long a_value);
            static void                      EnsureIntegerReplyGT (const ::ev::Object* a_object, const long long a_value);
            
            static const ::ev::redis::Value& EnsureArrayReply     (const ::ev::Object* a_object);
            static const ::ev::redis::Value& EnsureArrayReply     (const ::ev::Object* a_object, const size_t a_size);
            
            static void                      EnsureStatusValue      (const ::ev::redis::Value& a_object, const char* const a_value);
            
            static const ::ev::redis::Value& EnsureIntegerReply   (const ::ev::redis::Reply& a_reply);
            
            static void                      EnsureIntegerValueIsEQ (const ::ev::redis::Value& a_object, const long long a_value);
            static void                      EnsureIntegerValueIsGT (const ::ev::redis::Value& a_object, const long long a_value);
            static void                      EnsureIntegerValue     (const ::ev::redis::Value& a_object, const long long a_value);
            static void                      EnsureIntegerValue     (const ::ev::redis::Value& a_object, const long long a_value,
                                                                     const std::function<bool(const long long a_lhs, const long long a_rhs)> a_comparator);
      public: // Method(s) / Function(s)

          const Value& value () const;

       public: // Overloaded Operator(s)
                  
          void operator = (const struct redisReply* a_reply);
          
      }; // end of class 'Reply'

      /**
       * @return RO access to current value.
       */
      inline const Value& Reply::value () const
      {
          return value_;
      }
          
      /**
       * @brief Operator '=' overload.
       *
       * @param a_value \link struct redisReply \link to 'translate'.
       */
      inline void Reply::operator=(const struct redisReply* a_reply)
      {
          value_ = a_reply;
      }

    } // end of namespace 'redis'
    
} // end of namespace 'ev'

#endif // NRS_EV_REDIS_REPLY_H_
