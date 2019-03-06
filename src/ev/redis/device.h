/**
 * @file device.h - REDIS
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
#ifndef NRS_EV_REDIS_DEVICE_H_
#define NRS_EV_REDIS_DEVICE_H_

#include "ev/device.h"

#include <string>     // std::string
#include <functional> // std::function

#include <stdlib.h>

#include "ev/exception.h"

#include "ev/redis/includes.h"
#include "ev/redis/request.h"
#include "ev/redis/reply.h"

#include "ev/logger_v2.h"


namespace ev
{
    
    namespace redis
    {

        class Device final : public ev::Device, ev::LoggerV2::Client
        {
            
        protected: // Const Data
            
            const std::string  client_name_;       //!< REDIS client name.
            const std::string  ip_address_;        //!< REDIS server IP address.
            const int          port_number_;       //!< REDIS server port number address.
            const int          database_index_;    //!< REDIS database.
            
        private: // Data
            
            const Request*     request_ptr_;         //!< Pointer to the current request.
            redisAsyncContext* hiredis_context_;     //!< HIREDIS context.
            Request*           client_name_request_; //!<
            bool               client_name_set_;     //!<
            Request*           database_request_;    //!<
            bool               database_selected_;   //!<
            
        public: // Constructor(s) / Destructor
            
            Device (const Loggable::Data& a_loggable_data,
                    const std::string& a_client_name,
                    const char* const a_ip_address, const int a_port_number, const int a_database_index = -1);
            virtual ~Device ();
                    
        public: // Inherited Pure Virtual Method(s) / Function(s)

            virtual Status Connect         (ConnectedCallback a_callback);
            virtual Status Disconnect      (DisconnectedCallback a_callback);
            virtual Status Execute         (ExecuteCallback a_callback, const ev::Request* a_request);
            virtual Error* DetachLastError ();

        private:
            
            void SafeProcessReply                (const char* const a_function,
                                                  const ExecutionStatus& a_status, ev::Result* a_result,
                                                  const std::function<void(const ExecutionStatus& a_status, const Reply*)>& a_callback);
            bool ScheduleNextPostConnectCommand  ();
            void ClientNameSetCallback           (const ExecutionStatus& a_status,  ev::Result* a_result);
            void DatabaseIndexSelectionCallback  (const ExecutionStatus& a_status,  ev::Result* a_result);

        private: // Static Callbacks
            
            static void HiredisConnectCallback    (const struct redisAsyncContext* a_context, int a_status);
            static void HiredisDisconnectCallback (const struct redisAsyncContext* a_context, int a_status);
            static void HiredisDataCallback       (struct redisAsyncContext* a_context, void* a_reply, void* a_priv_data);

        }; // end of class 'Device'
        
    } // end of namespace 'redis'
    
} // end of namespace 'ev'

#endif // NRS_EV_REDIS_DEVICE_H_
