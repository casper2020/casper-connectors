/**
 * @file client.h
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

#ifndef NRS_CC_SOCKETS_DGRAM_IPC_CLIENT_H_
#define NRS_CC_SOCKETS_DGRAM_IPC_CLIENT_H_
#pragma once

#include "cc/singleton.h"

#include "osal/datagram_socket.h"

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
                class Client;
                class ClientInitializer final : public ::cc::Initializer<Client>
                {
                    
                public: // Constructor(s) / Destructor
                    
                    ClientInitializer (Client& a_Client);
                    virtual ~ClientInitializer ();
                    
                };
                
                // ---- //
                class Client final : public cc::Singleton<Client, ClientInitializer>
                {
                    
                    friend class ClientInitializer;
                    
                private: // Socket Related
                    
                    osal::DatagramClientSocket socket_;
                    std::string                socket_fn_;
                    
                private: // Helper(s)
                    
                    Json::FastWriter           fast_writer_;
                    
                public: // Method(s) / Function(s)
                    
                    void Start (const std::string& a_name, const std::string& a_runtime_directory);
                    void Send  (const Json::Value& a_value);

                }; // end of class 'Client'
                
            } // end of namespace 'ipc'
            
        } // end of namespace 'dgram'
        
    } // end of namespace 'sockets'
    
} // end of namespace 'cc'

#endif // NRS_CC_SOCKETS_DGRAM_IPC_CLIENT_H_
