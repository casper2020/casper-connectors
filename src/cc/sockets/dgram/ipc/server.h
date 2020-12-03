/**
 * @file server.h
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
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

#ifndef NRS_CC_SOCKETS_DGRAM_IPC_SERVER_H_
#define NRS_CC_SOCKETS_DGRAM_IPC_SERVER_H_
#pragma once

#include "cc/singleton.h"

#include "osal/datagram_socket.h"
#include "osal/condition_variable.h"

#include "cc/exception.h"

#include "cc/sockets/dgram/ipc/callback.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <list>

#include <event2/event.h> // libevent2

#include "json/json.h"

namespace cc
{
    
    namespace sockets
    {
        
        namespace dgram
        {
            
            namespace ipc
            {
                
                // ---- //
                class Server;
                class ServerInitializer final : public ::cc::Initializer<Server>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    ServerInitializer (Server& a_Server);
                    virtual ~ServerInitializer ();
                    
                };
                
                // ---- //
                class Server final : public cc::Singleton<Server, ServerInitializer>
                {
                    
                    friend class ServerInitializer;
                    
                public: // Data Type(s)
                    
                    typedef std::function<void(const Json::Value&)>     MessageCallback;
                    typedef std::function<void()>                       TerminatedCallback;
                    typedef std::function<void(const ::cc::Exception&)> FatalExceptionCallback;
                    typedef struct {
                        MessageCallback        on_message_received_;
                        TerminatedCallback     on_terminated_;
                        FatalExceptionCallback on_fatal_exception_;
                    } Callbacks;
                    
                private: // Threading Related
                    
                    std::thread*                 thread_;
                    std::atomic<bool>            running_;
                    std::atomic<bool>            aborted_;
                    std::atomic<bool>            thread_woken_;
                    std::mutex                   mutex_;
                    osal::ConditionVariable      thread_cv_;
                    osal::ConditionVariable      stop_cv_;
                    
                private: // Socket Related
                    
                    osal::DatagramServerSocket   socket_;
                    std::string                  socket_fn_;
                    
                    struct event_base*           event_base_;
                    struct event*                watchdog_event_;
                    
                    uint8_t*                     socket_buffer_;
                    size_t                       socket_buffer_length_;
                    
                private: // Callback Scheduling Related
                    
                    Callback*                    idle_callback_;
                    std::list<Callback*>         pending_callbacks_;
                    std::vector<Callback*>       active_callbacks_;
                    
                private:
                    
                    Callbacks*                   callbacks_;
                    
                public: // Method(s) / Function(s)
                    
                    void Start    (const std::string& a_name, const std::string& a_runtime_directory, const Callbacks& a_callbacks);
                    void Stop     (const int a_sig_no);
                    void Schedule (std::function<void()> a_function, const int64_t a_timeout_ms, const bool a_recurrent = false);
                    
                private: // Method(s) / Function(s)
                    
                    void Listen      ();
                    void OnDataReady ();
                    void Schedule    (const char* const a_caller);
                    
                private: // Static Method(s) / Function(s)
                    
                    static void DatagramEventHandlerCallback (evutil_socket_t a_fd, short a_flags, void* a_arg);
                    static void WatchdogCallback             (evutil_socket_t a_fd, short a_flags, void* a_arg);
                    static void ScheduledCallback            (evutil_socket_t a_fd, short a_flags, void* a_arg);
                    
                }; // end of class 'Server'
                
            } // end of namespace 'ipc'
            
        } // end of namespace 'dgram'
        
    } // end of namespace 'sockets'
    
} // end of namespace 'cc'

#endif // NRS_CC_SOCKETS_DGRAM_IPC_SERVER_H_
