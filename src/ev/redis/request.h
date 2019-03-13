/**
 * @file request.h - REDIS
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
#ifndef NRS_EV_REDIS_REQUEST_H_
#define NRS_EV_REDIS_REQUEST_H_

#include "ev/request.h"

#include "ev/redis/includes.h"

#include <string> // std::string
#include <vector>  // std::vector

namespace ev
{

    namespace redis
    {

        class Request : public ev::Request
        {

        public: // Enum(s)

            enum class Kind : uint8_t
            {
                Subscription,
                Other
            };

        protected: // Const Data

            const Kind kind_;     //!< Request kind, one of \link Kind \link.

        protected: // Data

            std::string payload_; //!< Request payload

        public: // Constructor(s) / Destructor

            Request(const Loggable::Data& a_loggable_data, const char* const a_command, const std::vector<std::string>& a_args);
            Request(const Loggable::Data& a_loggable_data, const ev::Request::Mode a_mode, const Kind a_kind);
            virtual ~Request();

        public: // Inherited Virtual Method(s) / Function(s)

            virtual const char*        AsCString () const;
            virtual const std::string& AsString  () const;

        public: // Method(s) / Function(s)

            Kind               kind       () const;

            void               SetPayload (const std::string& a_command, const std::vector<std::string>& a_args);
            const std::string& Payload    () const;
            const char* const  PayloadCStr() const;
            
        }; // end of class 'Request'

        /**
         * @return This request kind, one of \link Kind \link.
         */
        inline Request::Kind Request::kind () const
        {
            return kind_;
        }

        /**
         * @brief Set request payload ( REDIS command string ).
         *
         * @param a_command
         * @param a_args
         */
        inline void Request::SetPayload (const std::string& a_command, const std::vector<std::string>& a_args)
        {
            const size_t args_count = a_args.size() + 1;
            
            const char** args_values = new const char* [args_count];
            args_values[0] = a_command.c_str();
            for ( size_t idx = 0 ; idx < a_args.size() ; ++idx ) {
                args_values[ idx + 1 ] = a_args[idx].c_str();
            }
            
            sds cmd;
            if ( -1 != redisFormatSdsCommandArgv(&cmd, (int)args_count, args_values, nullptr) ) {
                payload_ = cmd;
            }
            
            delete [] args_values;
            
            sdsfree(cmd);
        }
        
        /**
         * @return Read-only access to the request payload ( REDIS command string ).
         */
        inline const std::string& Request::Payload () const
        {
            return payload_;
        }
        
        /**
         * @return Read-only access to the request payload ( REDIS command c-string ).
         */
        inline const char* const Request::PayloadCStr () const
        {
            return payload_.c_str();
        }

    } // end of namespace 'redis'

} // end of namespace 'ev'

#endif // NRS_EV_REDIS_REQUEST_H_
