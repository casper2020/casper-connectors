/**
 * @file client.cc
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

#include "cc/sockets/dgram/ipc/client.h"

#include "cc/exception.h"

#include "osal/osal_dir.h"

#ifdef __APPLE__
#pragma mark - ClientInitializer
#endif

/**
 * @brief This method will be called when it's time to initialize this singleton.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::sockets::dgram::ipc::ClientInitializer::ClientInitializer (cc::sockets::dgram::ipc::Client& a_instance)
    : ::cc::Initializer<cc::sockets::dgram::ipc::Client>(a_instance)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
cc::sockets::dgram::ipc::ClientInitializer::~ClientInitializer ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark - Server
#endif

/**
 * @brief Prepare this singleton instance.
 *
 * @parma a_name              Channel name.
 * @param a_runtime_directory Directory where socket file will be created.
 */
void cc::sockets::dgram::ipc::Client::Start (const std::string& a_name, const std::string& a_runtime_directory)
{
    if ( 0 != socket_fn_.length() ) {
        throw ::cc::Exception("Unable to start client communication channel: already running!");
    }
    socket_fn_ = a_runtime_directory + a_name + ".socket";
    
    if ( osal::Dir::EStatusOk != osal::Dir::CreatePath(a_runtime_directory.c_str()) ) {
        throw ::cc::Exception("Unable to create directory %s", a_runtime_directory.c_str());
    }
    
    if ( false == socket_.Create(socket_fn_) ) {
        // ... unable to create socket ...
        const auto exception = ::cc::Exception("Unable to start client communication channel: can't open a socket, using '%s' file!",
                                               socket_fn_.c_str()
        );
        socket_fn_ = "";
        throw exception;
    }
    // ... 'this' side socket must be binded now ...
    if ( false == socket_.Bind(false) ) {
        // ... unable to bind socket ...
        const auto exception = ::cc::Exception("Unable to bind client communication channel: %s !",
                                               socket_.GetLastConfigErrorString().c_str()
        );
        socket_fn_ = "";
        throw exception;
    }
}

/**
 * @brief Reset this singleton instance.
 *
 * @parma a_sig_no Signal number.
 */
void cc::sockets::dgram::ipc::Client::Stop (const int /* a_sig_no */)
{
    socket_.Close();
}

/**
 * @brief Prepare this singleton instance to send messages.
 *
 * @param a_value The JSON object to be sent.
 */
void cc::sockets::dgram::ipc::Client::Send (const Json::Value& a_value)
{
    if ( false == socket_.Send("%s", fast_writer_.write(a_value).c_str()) ) {
        if ( 34 != socket_.GetLastReceiveError() ) {
            throw ::cc::Exception("x) Unable to send message '%s' through socket: %s!", a_value.toStyledString().c_str(), socket_.GetLastSendErrorString().c_str());
        } else {
            // TODO CW : reschedule message
            for ( auto idx = 0 ; idx < 10 ; ++idx ) {
                fprintf(stderr, "TODO: handle with %s\n", socket_.GetLastSendErrorString().c_str());
            }
        }
    }
}
