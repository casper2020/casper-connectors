/**
 * @file server.cc
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

#include "cc/sockets/dgram/ipc/server.h"

#include <signal.h> // pthread_sigmask, sigemptyset, etc

#include "osal/osal_dir.h"

#ifdef __APPLE__
#pragma mark - ServerInitializer
#endif

/**
 * @brief This method will be called when it's time to initialize this singleton.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::sockets::dgram::ipc::ServerInitializer::ServerInitializer (cc::sockets::dgram::ipc::Server& a_instance)
    : ::cc::Initializer<cc::sockets::dgram::ipc::Server>(a_instance)
{
    instance_.thread_               = nullptr;
    instance_.running_              = false;
    instance_.aborted_              = false;
    instance_.thread_woken_         = false;
    instance_.event_base_           = nullptr;
    instance_.watchdog_event_       = nullptr;
    instance_.socket_buffer_        = nullptr;
    instance_.socket_buffer_length_ = 0;
    instance_.callbacks_            = nullptr;
    instance_.idle_callback_        = nullptr;
}

/**
 * @brief Destructor.
 */
cc::sockets::dgram::ipc::ServerInitializer::~ServerInitializer ()
{
    instance_.Stop(/* a_sig_no */ -1);
}

#ifdef __APPLE__
#pragma mark - Server
#endif

/**
 * @brief Start a new thread that will listen to client messages.
 *
 * @param a_name
 * @param a_runtime_directory
 * @param a_callbacks A set of functions to be called.
 */
void cc::sockets::dgram::ipc::Server::Start (const std::string& a_name, const std::string& a_runtime_directory,
                                             const cc::sockets::dgram::ipc::Server::Callbacks& a_callbacks)
{
    try {
        
        aborted_      = false;
        thread_woken_ = false;
        
        socket_fn_ = a_runtime_directory + a_name + ".socket";
        
        if ( osal::Dir::EStatusOk != osal::Dir::CreatePath(a_runtime_directory.c_str()) ) {
            throw ::cc::Exception("Unable to create directory %s", a_runtime_directory.c_str());
        }

        if ( nullptr != thread_ ) {
            throw ::cc::Exception("Unable to start server loop: already running!");
        }
        
        if ( nullptr != event_base_ ) {
            event_base_free(event_base_);
        }
        event_base_ = event_base_new(); // can return nullptr!
        if ( nullptr == event_base_ ) {
            throw ::cc::Exception("Unable to start hub loop: can't create 'base' event!");
        }
        
        if ( nullptr != watchdog_event_ ) {
            event_free(watchdog_event_);
        }
        watchdog_event_ = evtimer_new(event_base_, cc::sockets::dgram::ipc::Server::WatchdogCallback, this);
        
        timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 0;

        const int wd_rv = evtimer_add(watchdog_event_, &tv);
        if ( wd_rv < 0 ) {
            throw ::cc::Exception("Unable to start server loop: can't add 'watchdog' event - error code %d !",
                                  wd_rv
            );
        }
        
        if ( nullptr != idle_callback_ ) {
            delete idle_callback_;
        }
        idle_callback_ = new cc::sockets::dgram::ipc::Callback(this, [this] {
            Schedule(__FUNCTION__);
        },
        /* a_timeout_ms */ 2000, /* a_recurrent */ true);
        idle_callback_->SetTimer(event_base_, cc::sockets::dgram::ipc::Server::ScheduledCallback);
        
        socket_buffer_        = new uint8_t[4096];
        socket_buffer_length_ = 4096;
        
        thread_    = new std::thread(&cc::sockets::dgram::ipc::Server::Listen, this);
        callbacks_ = new cc::sockets::dgram::ipc::Server::Callbacks({
            /* on_message_received_ */ a_callbacks.on_message_received_,
            /* on_terminated_       */ a_callbacks.on_terminated_,
            /* on_fatal_exception_  */ a_callbacks.on_fatal_exception_
        });
        
        thread_->detach();
        thread_cv_.Wait();
        
    } catch (const ::cc::Exception& a_cc_exception) {
        mutex_.lock();
        if ( nullptr != callbacks_ && nullptr != callbacks_->on_fatal_exception_ ) {
            callbacks_->on_fatal_exception_(a_cc_exception);
        }
        mutex_.unlock();
    }
}

/**
 * @brief Stop the currenlty running thread ( if any ).
 *
 * @param a_sig_no The signal that wants to stop this.
 */
void cc::sockets::dgram::ipc::Server::Stop (const int /* a_sig_no */)
{
    aborted_ = true;
    
    if ( true == running_ ) {
        if ( nullptr != event_base_ ) {
            event_active(watchdog_event_, EV_TIMEOUT, 0);
        }
        stop_cv_.Wait();
    }
    
    while ( active_callbacks_.size() > 0 ) {
        delete active_callbacks_.back();
        active_callbacks_.pop_back();
    }
    
    running_ = false;
    
    if ( nullptr != watchdog_event_ ) {
        event_free(watchdog_event_);
        watchdog_event_ = nullptr;
    }
    
    if ( nullptr != idle_callback_ ) {
        delete idle_callback_;
        idle_callback_ = nullptr;
    }

    if ( nullptr != event_base_ ) {
        event_base_free(event_base_);
        event_base_ = nullptr;
    }
    
    if ( nullptr != thread_ ) {
        delete thread_;
        thread_ = nullptr;
    }
    
    if ( nullptr != callbacks_ ) {
        delete callbacks_;
        callbacks_ = nullptr;
    }
    
    socket_.Close();
    
    if ( nullptr != socket_buffer_ ) {
        delete [] socket_buffer_;
        socket_buffer_ = nullptr;
        socket_buffer_length_ = 0;
    }
}

/**
 * @brief Schedule a one-shot callback.
 *
 * @param a_function Callback function.
 * @param a_timeout_ms Timeout in milliseconds.
 * @param a_recurrent  When true this event will repeat every a_timeout_ms milliseconds.
 */
void cc::sockets::dgram::ipc::Server::Schedule (std::function<void()> a_function, const int64_t a_timeout_ms, const bool a_recurrent)
{
    mutex_.lock();
    pending_callbacks_.push_back(new cc::sockets::dgram::ipc::Callback(this, a_function, a_timeout_ms, a_recurrent));
    mutex_.unlock();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief A loop to wait for incoming messages.
 */
void cc::sockets::dgram::ipc::Server::Listen ()
{
    running_ = true;
    
    struct event* socket_event_ = nullptr;
    
    sigset_t sigmask;
    sigset_t saved_sigmask;
    
    // ... reset all signals ...
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);

    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGUSR2);
    sigaddset(&sigmask, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &sigmask, &saved_sigmask);

#ifdef __APPLE__
    pthread_setname_np("IPC Server");
#endif
    
    try {

        if ( false == socket_.Create(socket_fn_.c_str()) ) {
            throw ::cc::Exception("Can't open a socket, using %s file: %s!",
                                  socket_fn_.c_str(), socket_.GetLastConfigErrorString().c_str()
            );
        }
        
        if ( false == socket_.Bind() ) {
            throw ::cc::Exception("Unable to bind server socket (%s): %s!",
                                  socket_fn_.c_str(), socket_.GetLastConfigErrorString().c_str()
            );
        }
        
        if ( false == socket_.SetNonBlock() ) {
            throw ::cc::Exception("Unable to set socket non-block property: %s!",
                                  socket_.GetLastConfigErrorString().c_str()
            );
        }
        
        socket_event_ = event_new(event_base_, socket_.GetFileDescriptor(), EV_READ | EV_PERSIST, DatagramEventHandlerCallback, this);
        if ( nullptr == socket_event_ ) {
            throw ::cc::Exception("Unable to create an event for datagram socket!");
        }
        
        timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 20000;
        
        if ( 0 != event_add(socket_event_, &tv) ) {
            throw ::cc::Exception("Unable to add datagram socket event!");
        }
        
#ifndef EVLOOP_NO_EXIT_ON_EMPTY
    #define EVLOOP_NO_EXIT_ON_EMPTY 0x04
#endif

        while ( false == aborted_ ) {
        
            int rv = event_base_loop(event_base_, EVLOOP_NO_EXIT_ON_EMPTY);
            switch (rv) {
                case -1: // ... error ...
                    break;
                case  0: // ... success ...
                    break;
                case  1: // ... no events registered ...
                    break;
                default:
                    break;
            }
            
        }
        
        socket_.Close();
        
    } catch (const ::cc::Exception& a_cc_exception) {
        
        thread_cv_.Wake();

        mutex_.lock();
        callbacks_->on_fatal_exception_(a_cc_exception);
        mutex_.unlock();
    }

    running_ = false;
    
    mutex_.lock();
    if ( nullptr != callbacks_ && nullptr != callbacks_->on_terminated_ ) {
        callbacks_->on_terminated_();
    }
    mutex_.unlock();

    // ... restore the signal mask ...
    pthread_sigmask(SIG_SETMASK, &saved_sigmask, NULL);

    stop_cv_.Wake();
}

/**
 * @brief This method will be called when we've data to read.
 */
void cc::sockets::dgram::ipc::Server::OnDataReady ()
{
    
    try {

        Json::Reader reader;
        Json::Value  value;

        // ... while bytes available ...
        while ( true ) {
            
            size_t length = 0;
            
            if ( false == socket_.Receive(socket_buffer_, socket_buffer_length_, length) ) {
                
                const int last_error = socket_.GetLastReceiveError();
                if ( EAGAIN == last_error ) {
                    break;
                } else if ( 0 == last_error ) { // no messages are available to be received and the peer has performed an orderly shutdown
                    throw ::cc::Exception("No messages are available to be received and the peer has performed an orderly shutdown!");
                } else {
                    throw ::cc::Exception("Unable to read data from socket : %d - %s!",
                                          last_error,
                                          socket_.GetLastReceiveErrorString().c_str()
                    );
                }
                
            }

            const std::string message = std::string(reinterpret_cast<char*>(socket_buffer_), length);
            
            if ( false == reader.parse(message, value) ) {
                const auto errors = reader.getStructuredErrors();
                if ( errors.size() > 0 ) {
                    throw ::cc::Exception("An error occurred while parsing received JSON message: %s!",
                            errors[0].message.c_str()
                    );
                } else {
                    throw ::cc::Exception("An error occurred while parsing received JSON message!");
                }
            }
            
            mutex_.lock();
            callbacks_->on_message_received_(value);
            mutex_.unlock();
            
        }
        
    } catch (const ::cc::Exception& a_cc_exception) {
        mutex_.lock();
        callbacks_->on_fatal_exception_(a_cc_exception);
        mutex_.unlock();
    }
    
}


/**
 * @brief Process pending callbacks events registration.
 *
 * @param a_caller
 */
void cc::sockets::dgram::ipc::Server::Schedule (const char* const /* a_caller */)
{
    mutex_.lock();
    
    while ( pending_callbacks_.size() > 0 ) {
        
        cc::sockets::dgram::ipc::Callback* callback = pending_callbacks_.front();

        callback->SetTimer(event_base_, cc::sockets::dgram::ipc::Server::ScheduledCallback);
        
        active_callbacks_.push_back(callback);
        
        pending_callbacks_.pop_front();
    }
    
    mutex_.unlock();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Data callback.
 *
 * param a_fd    Unused.
 * param a_flags Unused.
 * @param a_arg  Pointer to singleton instance.
 */
void cc::sockets::dgram::ipc::Server::DatagramEventHandlerCallback (evutil_socket_t a_fd, short /* a_flags */, void* a_arg)
{    
    auto self = static_cast<cc::sockets::dgram::ipc::Server*>(a_arg);
    
    if ( false == self->thread_woken_ ) {
        self->thread_woken_ = true;
        self->thread_cv_.Wake();
        return;
    }

    if ( self->socket_.GetFileDescriptor() != a_fd /* || a_flags != EV_READ */ ) {
        return;
    }

    self->OnDataReady();
    
    self->Schedule(__FUNCTION__);
}

/**
 * @brief Callback to handle break base.
 *
 * param a_fd    Unused.
 * param a_flags Unused.
 * @param a_arg  Pointer to singleton instance.
 */
void cc::sockets::dgram::ipc::Server::WatchdogCallback (evutil_socket_t /* a_fd */, short /* a_flags */, void* a_arg)
{
    cc::sockets::dgram::ipc::Server* self = (cc::sockets::dgram::ipc::Server*)a_arg;
    if ( true == self->aborted_ ) {
        event_base_loopbreak(self->event_base_);
    } else {
        timeval tv;
        tv.tv_sec  = 365 * 24 * 3600;
        tv.tv_usec = 0;
        (void)evtimer_add(self->watchdog_event_, &tv);
    }
    self->Schedule(__FUNCTION__);
}

/**
 * @brief Callback to handle scheduled events.
 *
 * param a_fd    Unused.
 * param a_flags Unused.
 * @param a_arg  Pointer to singleton instance.
 */
void cc::sockets::dgram::ipc::Server::ScheduledCallback (evutil_socket_t /* a_fd */, short /* a_flags */, void* a_arg)
{
    cc::sockets::dgram::ipc::Callback* callback = (cc::sockets::dgram::ipc::Callback*)a_arg;
    bool erase = false;
    try {
        callback->Call();
        erase = ( false == callback->recurrent_);
    } catch (const ::cc::Exception& a_cc_exception) {
        // TODO CW: notify owner
        erase = true;
    }
    
    if ( true == erase ) {
        cc::sockets::dgram::ipc::Server* server = const_cast<cc::sockets::dgram::ipc::Server*>(static_cast<const cc::sockets::dgram::ipc::Server*>(callback->owner_));
        server->mutex_.lock();
        for ( auto it = server->active_callbacks_.begin(); server->active_callbacks_.end() != it ; ++it ) {
            if ( (*it) == callback ) {
                server->active_callbacks_.erase(it);
                break;
            }
        }
        server->mutex_.unlock();
    }
}
