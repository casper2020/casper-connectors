/**
 * @file session.h - REDIS
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
#ifndef NRS_EV_REDIS_SESSION_H_
#define NRS_EV_REDIS_SESSION_H_

#include "ev/exception.h"
#include "ev/scheduler/scheduler.h"
#include "ev/loggable.h"

#include "ev/redis/reply.h"

#include <string>     // std::string
#include <map>        // std::map
#include <functional> // std::function

namespace ev
{
    
    namespace redis
    {
        
        class Session : public ::ev::scheduler::Scheduler::Client
        {
            
        public: // Data Type(s)
            
            class DataT
            {

            public: // Data
                
                std::string                        provider_;
                std::string                        user_id_;
                std::string                        token_;
                bool                               token_is_valid_;
                std::map<std::string, std::string> payload_;
                int64_t                            expires_in_;     //!< in seconds
                bool                               verified_;
                bool                               exists_;
                
            public: // Constructor(s) / Destructor
                
                /**
                 * @brief Default constructor.
                 */
                DataT () {
                    token_is_valid_ = false;
                    expires_in_     = -1;
                    verified_       = false;
                    exists_         = false;
                }
                
                /**
                 * @brief Copy constructor.
                 */
                DataT (const DataT& a_data) {
                    provider_       = a_data.provider_;
                    user_id_        = a_data.user_id_;
                    token_          = a_data.token_;
                    token_is_valid_ = a_data.token_is_valid_;
                    for ( auto it : a_data.payload_ ) {
                        payload_[it.first] = it.second;
                    }
                    expires_in_      = a_data.expires_in_;
                    verified_        = a_data.verified_;
                    exists_          = a_data.exists_;
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~DataT ()
                {
                    /* emtpy */
                }
                
            public: // Operators
                
                /**
                 * @brief Overloaded '=' operator.
                 */
                inline void operator = (const DataT& a_data)
                {
                    provider_       = a_data.provider_;
                    user_id_        = a_data.user_id_;
                    token_          = a_data.token_;
                    token_is_valid_ = a_data.token_is_valid_;
                    for ( auto it : a_data.payload_ ) {
                        payload_[it.first] = it.second;
                    }
                    expires_in_      = a_data.expires_in_;
                    verified_        = a_data.verified_;
                    exists_          = a_data.exists_;
                }

            };
            
            typedef std::function<void(const DataT& a_data)>                                      SuccessCallback;
            typedef std::function<void(const DataT& a_data)>                                      InvalidCallback;
            typedef std::function<void(const DataT& a_data, const ev::Exception& a_ev_exception)> FailureCallback;
            
        protected: // Const Data
            
            const std::string iss_;
            const std::string token_prefix_;
            
        protected: // Data
            
            Loggable::Data loggable_data_;
            
            DataT          data_;
            bool           reverse_track_enabled_;

        public: // Constructor(s) / Destructor
            
            Session (const Loggable::Data& a_loggable_data,
                     const std::string& a_iss, const std::string& a_token_prefix);
            Session (const Session& a_session);
            virtual ~Session();
            
        public: // Method(s) / Function(s)
            
            void         Set          (const DataT& a_data,
                                       const SuccessCallback a_success_callback, const FailureCallback a_failure_callback);
            void         Unset        (const DataT& a_data,
                                       const SuccessCallback a_success_callback, const FailureCallback a_failure_callback);
            void         Fetch        (const SuccessCallback a_success_callback, const InvalidCallback a_invalid_callback, const FailureCallback a_failure_callback);
            
            const DataT& Data         () const;
            void         SetToken     (const std::string& a_token);
            
            
        public:
            
            static std::string Random (uint8_t a_length);
            static bool        IsRandomValid (const std::string& a_value, uint8_t a_length);
            
        private: // Method(s) / Function(s)
            
            ::ev::scheduler::Task* NewTask (const EV_TASK_PARAMS& a_callback);
            
        }; // end of class 'Session'
        
        /**
         * @return Read-only access to session data, see \link Session::Data \link.
         */
        inline const Session::DataT& Session::Data () const
        {
            return data_;
        }
        
        /**
         * @brief Set the current session token and invalidate all previously collected data ( if any ).
         *       A subsequent call to \link Fetch \link must be done in order to load token payload from REDIS.
         *
         * @param a_token New token string.
         */
        inline void Session::SetToken (const std::string& a_token)
        {
            data_.provider_       = "";
            data_.user_id_        = "";
            data_.token_          = a_token;
            data_.token_is_valid_ = false;
            data_.payload_.clear();
            data_.expires_in_     = -1;
            data_.verified_       = false;
            data_.exists_         = false;
        }
        
    } // end of namespace 'redis'
    
} // end of namespace 'ev'

#endif // NRS_EV_REDIS_SESSION_H_
