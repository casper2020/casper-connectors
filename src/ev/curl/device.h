/**
 * @file device.h
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
#ifndef NRS_EV_CURL_DEVICE_H_
#define NRS_EV_CURL_DEVICE_H_

#include "ev/device.h"

#include "ev/curl/request.h"

#include <curl/curl.h>

#include <map>

namespace ev
{

    namespace curl
    {
        /**
         * @brief A class that defines a 'CURL device' that connects to 'hub'.
         */
        class Device : public ev::Device
        {

        private: // Data Type(s)
            
            class MultiContext
            {

            public: // Data Type(s)

                class SocketContext
                {
                    

                public: // Data

                    curl_socket_t fd_;
                    long          timeout_;
                    int           event_action_;
                    struct event* event_;
                    CURL*         easy_handle_ptr_;
                    MultiContext* context_ptr_;
                    Request*      request_ptr_;

                public: // Constructor(s) / Destructor

                    SocketContext ();
                    virtual ~SocketContext ();

                }; // end of class 'SocketContext'

            public: // Data

                Device*       device_ptr_;            //!<
                CURLM*        handle_;                //!< CURL multi handle.
                CURLMcode     last_code_;             //!<
                CURLcode      last_exec_code_;        //!<
                int           last_http_status_code_; //!<
                int           setup_errors_;          //!<
                int           still_running_;         //!<
                struct event* event_;                 //!< Libevent context.
                struct event* timer_event_;           //!<

            public: // Constructor(s) / Destructor

                MultiContext (Device* a_device);
                virtual ~MultiContext ();

            public:

                void Process        (MultiContext* a_context, CURLMcode a_code, const char* a_where);
                bool ContainsErrors () const;

            public: // Static Method(s) / Function(s)

                static int  SocketCallback (CURL* a_handle, curl_socket_t a_socket, int a_what, void* a_user_ptr, void* a_socket_ptr);
                static int  TimerCallback  (CURLM* a_handle, long a_timeout_ms, void* a_user_ptr);

                static void EventCallback      (int a_fd, short a_kind, void* a_context);
                static void EventTimerCallback (int a_fd, short a_kind, void* a_context);

            };

            typedef struct _RequestContext {
                Request*        request_ptr_;
                ExecuteCallback exec_callback_;
            } RequestContext;
            typedef std::map<CURL*, RequestContext> RequestsMap;      

        private: // Data

            MultiContext* context_; //!< CURL context, see \link MultiContext \link.
            RequestsMap   map_;     //!< Request -> RequestContext

        public: // Constructor(s) / Destructor

            Device (const Loggable::Data& a_loggable_data);
            virtual ~Device ();

        public: // Inherited Pure Virtual Method(s) / Function(s)

            virtual Status Connect         (ConnectedCallback a_callback);
            virtual Status Disconnect      (DisconnectedCallback a_callback);
            virtual Status Execute         (ExecuteCallback a_callback, const ev::Request* a_request);
            virtual Error* DetachLastError ();

        private: // Method(s) / Function(s)

            void Disconnect (bool a_notify = true);

        }; // end of class 'Device'

    } // end of namespace 'curl'

} // end of namespace 'ev'

#endif // NRS_EV_CURL_DEVICE_H_
