/**
 * @file device.h - PostgreSQL
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
#ifndef NRS_EV_POSTGRESQL_DEVICE_H_
#define NRS_EV_POSTGRESQL_DEVICE_H_

#include "ev/device.h"

#include "json/json.h"

#include <string>     // std::string

#include <stdlib.h>

#include <libpq-fe.h>

#include <chrono>

namespace ev
{
    
    namespace postgresql
    {
        /**
         * @brief A class that defines a 'PostgreSQL device' that connects to 'hub'.
         */
        class Device : public ev::Device
        {
            
        private: // Data
            
            class PostgreSQLContext
            {
                
            public: // Data

                std::string                           query_;                 //!<
                Loggable::Data                        loggable_data_;         //!<
                void*                                 device_ptr_;            //!< Pointer to the owner of this context.
                PGconn*                               connection_;            //!< PostgreSQL connection, nullptr if none.
                struct timeval                        connection_timeout_;    //!< connection timeout
                PGPing                                ping_;                  //!< Last ping result, one of \link PGPing \link.
                struct event*                         event_;                 //!< Libevent context.
                bool                                  statement_timeout_set_; //!<
                Result*                               pending_result_;        //!<
                std::chrono::steady_clock::time_point exec_start_;
                std::string                           last_connection_status_;
                std::string                           last_reported_connection_status_;
                std::chrono::steady_clock::time_point connection_scheduled_tp_;
                std::chrono::steady_clock::time_point connection_established_tp_;
                std::chrono::steady_clock::time_point connection_finished_tp_;
                bool                                  connection_established_;
                
            public: // Constructor(s) / Destructor

                /**
                 * @brief Default constructor.
                 *
                 * @param a_device_ptr
                 */
                PostgreSQLContext (void* a_device_ptr)
                {
                    device_ptr_                 = a_device_ptr;
                    connection_                 = nullptr;
                    connection_timeout_.tv_sec  = 15;
                    connection_timeout_.tv_usec = 0;
                    ping_                       = PGPing::PQPING_OK;
                    event_                      = nullptr;
                    statement_timeout_set_      = false;
                    pending_result_             = nullptr;
                    exec_start_                 = std::chrono::steady_clock::now();
                    connection_established_     = false;
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~PostgreSQLContext ()
                {
                    // ... connection variable is managed by this class owner ...
                    if ( nullptr != event_ ) {
                        event_free(event_);
                    }
                    if ( nullptr != pending_result_ ) {
                        delete pending_result_;
                    }
                }
                
            };
            
            PostgreSQLContext* context_;                      //!< PostgreSQL context, see \link PostgreSQLContext \link.
            std::string        connection_string_;            //!< PostgreSQL connection string.
            int                statement_timeout_;            //!< PostgreSQL statement timeout.
            Json::Value        post_connect_queries_;         //!<
            bool               post_connect_queries_applied_; //!<

        public: // Constructor(s) / Destructor
            
            Device (const Loggable::Data& a_loggable_data,
                    const char* const a_conn_str, const int a_statement_timeout, const Json::Value& a_post_connect_queries,
                    const ssize_t a_max_queries_per_conn);
            virtual ~Device ();
            
        public: // Inherited Pure Virtual Method(s) / Function(s)
            
            virtual Status Connect         (ConnectedCallback a_callback);
            virtual Status Disconnect      (DisconnectedCallback a_callback);
            virtual Status Execute         (ExecuteCallback a_callback, const ev::Request* a_request);
            virtual Error* DetachLastError ();
            
        private: // Method(s) / Function(s)
            
            void Disconnect ();
            
        private: // Static Callbacks
            
            static void PostgreSQLEVCallback (evutil_socket_t a_fd, short /* a_flags */, void* a_arg);
            
        private: // Inline Method(s) / Function(s)
            
            std::string GetExecStatusTypeString (const ExecStatusType a_type) const;

        }; // end of class 'Device'
        
        /*
         * @return \link ExecStatusType \link string representation.
         */
        inline std::string Device::GetExecStatusTypeString (const ExecStatusType a_type) const
        {
            switch(a_type) {
                case PGRES_EMPTY_QUERY:    /* 0 - empty query string was executed */
                    return "PGRES_EMPTY_QUERY";
                case PGRES_COMMAND_OK:     /* 1 - a query command that doesn't return anything was executed properly by the backend */
                    return "PGRES_COMMAND_OK";
                case PGRES_TUPLES_OK:      /* 2 - a query command that returns tuples was executed properly by the backend, PGresult contains the result tuples */
                    return "PGRES_TUPLES_OK";
                case PGRES_COPY_OUT:       /* 3 - Copy Out data transfer in progress */
                    return "PGRES_COPY_OUT";
                case PGRES_COPY_IN:        /* 4 - Copy In data transfer in progress */
                    return "PGRES_COPY_IN";
                case PGRES_BAD_RESPONSE:   /* 5 - an unexpected response was recv'd from the backend */
                    return "PGRES_BAD_RESPONSE";
                case PGRES_NONFATAL_ERROR: /* 6 - notice or warning message */
                    return "PGRES_NONFATAL_ERROR";
                case PGRES_FATAL_ERROR:    /* 7 - query failed */
                    return "PGRES_FATAL_ERROR";
                case PGRES_COPY_BOTH:      /* 8 - Copy In/Out data transfer in progress */
                    return "PGRES_COPY_BOTH";
                case PGRES_SINGLE_TUPLE:   /* 9 - single tuple from larger resultset */
                    return "PGRES_SINGLE_TUPLE";
                default:
                    return "??? ~> " + std::to_string(a_type);
            }
        }
        
    } // end of namespace 'postgresql'
    
} // end of namespace 'ev'

#endif // NRS_EV_POSTGRESQL_DEVICE_H_
