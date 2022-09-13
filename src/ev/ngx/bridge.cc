/**
 * @file bridge.cc
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

#include "ev/ngx/bridge.h"

#include "osal/osalite.h"

#include <mutex> // std::mutex

#include <sstream> // std::stringstream

ngx_connection_t* ev::ngx::Bridge::connection_         = nullptr;
ngx_event_t*      ev::ngx::Bridge::event_              = nullptr;
ngx_log_t*        ev::ngx::Bridge::log_                = nullptr;
uint8_t*          ev::ngx::Bridge::buffer_             = nullptr;
size_t            ev::ngx::Bridge::buffer_length_      = 0;
size_t            ev::ngx::Bridge::buffer_bytes_count_ = 0;

/**
 * @brief One-shot initializer.
 *
 * @param a_socket_fn
 * @param a_fatal_exception_callback
 */
void ev::ngx::Bridge::Startup (const std::string& a_socket_fn,
                               std::function<void(const ev::Exception& a_ev_exception)> a_fatal_exception_callback)
{
    OSALITE_DEBUG_TRACE("ev_ngx_shared_handler", "~> Startup(...)");
    // ... ngx sanity check ...
    if ( nullptr == ngx_cycle ) {
        // ... ngx not ready ...
        throw ev::Exception("Invalid startup call - ngx_cycle not set!");
    }
    
    // ... sanity check ...
    if ( nullptr != connection_ || nullptr != event_ || nullptr != log_ ) {
        // ... insane call ...
        throw ev::Exception("Already initialized!");
    }

    pending_callbacks_count_ = 0;

    // ... allocate a new buffer ...
    buffer_        = new uint8_t[1024];
    buffer_length_ = 1024;
    
    //
    // SOCKET
    //
    
    // ... create socket ...
    if ( false == socket_.Create(a_socket_fn) ) {
        throw ev::Exception("Can't open a socket, using '%s' file: %s!", a_socket_fn.c_str(), socket_.GetLastConfigErrorString().c_str());
    }

    // ... 'this' side socket must be binded now ...
    if ( false == socket_.Bind() ) {
      // ... unable to bind socket ...
      throw ev::Exception("Unable to bind client: %s", socket_.GetLastConfigErrorString().c_str());
    }

    // ... set non-block ...
    if ( false == socket_.SetNonBlock() ) {
      throw ev::Exception("Unable to set socket non-block property:  %s", socket_.GetLastConfigErrorString().c_str());
    }
    
    //
    // LOG
    //
    log_ = (ngx_log_t*)malloc(sizeof(ngx_log_t));
    if ( nullptr == log_ ) {
        throw ev::Exception("Unable to create 'shared handler' log!");
    }
    ngx_memzero(log_, sizeof(ngx_log_t));
    
    //
    // CONNECTION
    //
    connection_ = ngx_get_connection((ngx_socket_t)socket_.GetFileDescriptor(), log_);
    if ( nullptr == connection_ ) {
        throw ev::Exception("Unable to create 'shared handler' connection!\n");
    }
    connection_->write->log = log_;
    connection_->read->log  = log_;
    connection_->recv       = ev::ngx::Bridge::Receive;
    connection_->send       = ev::ngx::Bridge::Send;
    
    //
    // EVENT
    //
    event_ = (ngx_event_t*)malloc(sizeof(ngx_event_t));
    if ( NULL == event_ ) {
        throw ev::Exception("Unable to create 'shared handler' event!\n");
    }
    // ... just keeping the same behavior as in ngx_pcalloc ...
    ngx_memzero(event_, sizeof(ngx_event_t));
    
    /* UDP sockets are always ready to write */
    event_->ready   = 1;
    
    event_->log     = log_;
    event_->handler = ev::ngx::Bridge::Handler;
    event_->data    = connection_;

    //
    // nginx v1.11.4
    // epoll and kqueue have different ways ( implementations ) to handle read / write events
    //
#if defined(NGX_USE_EPOLL_EVENT)
    // ... assuming and tested at linux ...
    connection_->write = event_;
    connection_->read  = event_;
#elif defined(NGX_HAVE_KQUEUE)
    // ... no overrides required ...
#else
    // ... not ready ...
    #error "Don't know how to setup nginx event!"
#endif

    ngx_uint_t flags = (ngx_event_flags & NGX_USE_CLEAR_EVENT) ?
    /* kqueue, epoll */                  NGX_CLEAR_EVENT:
    /* select, poll, /dev/poll */        NGX_LEVEL_EVENT;
    
    ngx_int_t ngx_add_rv;
    if ( NGX_OK != ( ngx_add_rv = ngx_add_event(event_, NGX_READ_EVENT, flags) ) ) {
        throw ev::Exception("Unable to add 'shared handler' event: %ld!\n", ngx_add_rv);
    }

    // ... reset stats data ...
    pending_callbacks_count_ = 0;
    // ... keep track of callbacks ...
    fatal_exception_callback_ = a_fatal_exception_callback;
    
    OSALITE_DEBUG_TRACE("ev_ngx_shared_handler", "<~ Startup(...) - connection_=%p, event_=%p, socket[ %d] %s",
						(void*)connection_, (void*)event_,
						socket_.GetFileDescriptor(), a_socket_fn.c_str()
	);
}

/**
 * @brief Dealloc previously allocated memory ( if any ).
 */
void ev::ngx::Bridge::Shutdown ()
{
    OSALITE_DEBUG_TRACE("ev_ngx_shared_handler", "~> Shutdown()");
    // ... finally, shutdown 'ev ngx glue' ...
    if ( nullptr != event_ ) {
        ngx_del_event(event_, NGX_READ_EVENT, 0);
        free(event_);
        event_ = nullptr;
    }
    if ( nullptr != connection_ ) {
        ngx_free_connection(connection_);
        connection_ = nullptr;
    }
    if ( nullptr != log_ ) {
        free(log_);
        log_ = nullptr;
    }
    if ( nullptr != buffer_ ) {
        delete [] buffer_;
        buffer_        = nullptr;
        buffer_length_ = 0;
    }
    // ... loose refs ...
    fatal_exception_callback_ = nullptr;
    
    OSALITE_DEBUG_TRACE("ev_ngx_shared_handler", "<~ Shutdown()");
}

#ifdef __APPLE__
#pragma mark - Inherited Virtual Method(s) / Function(s) - from ::ev::SharedHandler
#endif

/**
 * @brief Call this method to perform a callback on main thread.
 *
 * @param a_callback
 * @param a_payload
 * @param a_timeout
 */
void ev::ngx::Bridge::CallOnMainThread (std::function<void(void* a_payload)> a_callback, void* a_payload, int64_t a_timeout_ms)
{
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);

    ScheduleCalbackOnMainThread(new ev::ngx::Bridge::Callback(a_callback, a_payload, a_timeout_ms), a_timeout_ms);
}

/**
 * @brief Call this method to perform a callback on main thread.
 *
 * @param a_callback
 * @param a_timeout
 */
void ev::ngx::Bridge::CallOnMainThread (std::function<void()> a_callback, int64_t a_timeout_ms)
{
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);

    ScheduleCalbackOnMainThread(new ev::ngx::Bridge::Callback(a_callback, a_timeout_ms), a_timeout_ms);
}

/**
 * @brief Call this method handle with a fatal exception.
 */
void ev::ngx::Bridge::ThrowFatalException (const ev::Exception& a_ev_exception)
{
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);

    fatal_exception_callback_(a_ev_exception);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Schedule a callback on 'main' thread.
 *
 * @param a_callback
 * @param a_timeout_ms
 */
void ev::ngx::Bridge::ScheduleCalbackOnMainThread (ev::ngx::Bridge::Callback* a_callback, int64_t a_timeout_ms)
{
    OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                        "smt: ~> scheduling callback %p...",
                        a_callback
    );
    
    if ( 0 == a_timeout_ms ) {
        // ... keep track of # of pending callbacks ...
        const int remaining = std::atomic_fetch_add(&pending_callbacks_count_,1);
        // ... send message through socket to be read at 'main' thread ...
        if ( false == socket_.Send("callback:%p", static_cast<void*>(a_callback)) ) {
            if ( EAGAIN == socket_.GetLastSendError() ) {

				OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
									"smt: ~> (re)scheduling callback %p...",
									a_callback
				);

                ScheduleCalbackOnMainThread(a_callback, 1000);
                return;
            } else {
                throw ev::Exception("Unable to send a message through socket: %s!",
                                    socket_.GetLastSendErrorString().c_str()
                );
            }
        }
        OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                            "smt: ~> callback %p scheduled [ pending_callbacks_count_ = %d ]",
                            a_callback, remaining
        );
        (void)remaining;
    } else {
        // ... differed ...
        a_callback->ngx_event_ = (ngx_event_t*)malloc(sizeof(ngx_event_t));
        if ( NULL == a_callback->ngx_event_ ) {
            delete a_callback;
            throw ev::Exception("Unable to create 'shared handler' differed event!\n");
        }
        // ... just keeping the same behavior as in ngx_pcalloc ...
        ngx_memzero(a_callback->ngx_event_, sizeof(ngx_event_t));
        
        // ... set event ...
        a_callback->ngx_event_->log     = log_;
        a_callback->ngx_event_->handler = ev::ngx::Bridge::DifferedHandler;
        a_callback->ngx_event_->data    = a_callback;
        
        // ... keep track of # of pending callbacks ...
        const int remaining = std::atomic_fetch_add(&pending_callbacks_count_,1);
        
        // ... schedule a callback through socket to be called at 'main' thread ...
        ngx_add_timer(a_callback->ngx_event_, (ngx_msec_t)(a_timeout_ms));

        OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                            "smt: ~> callback %p scheduled [ pending_callbacks_count_ = %d ], timeout in " INT64_FMT "ms",
                            a_callback, remaining, a_timeout_ms
        );
        (void)remaining;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Handler called by the ngx event loop.
 *
 * @param a_event
 */
void ev::ngx::Bridge::Handler (ngx_event_t* a_event)
{
    ev::ngx::Bridge& handler = ev::ngx::Bridge::GetInstance();

	OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
						"sh: a_event->available=%d, a_event->ready=%d, connection_[%p]->fd=%d",
						a_event->available, a_event->ready,
						handler.connection_,
						nullptr != handler.connection_ ? handler.connection_->fd : -1
    );

    if ( a_event->available <= 0 || 0 == a_event->ready ) {
        return;
    }

    try {
        
        int msg_received  = 0;
        int rx_count      = 0;
        int callbacks_remaining = 0;
        
        // ... while bytes available ...
#ifdef __APPLE__
        while ( rx_count < a_event->available ) {
#else
        while ( 1 ) { // { rx_count < a_event->available ) {
#endif


            // ... read data from socket ...
            if ( false == handler.socket_.Receive(handler.buffer_, handler.buffer_length_, handler.buffer_bytes_count_) ) {
                // ... fatal error?
                const int         last_error_code = handler.socket_.GetLastReceiveError();
				OSALITE_DEBUG_IF("ev_ngx_shared_handler") {
					const std::string last_error_msg  = handler.socket_.GetLastReceiveErrorString();
					OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
										"sh: rx error %d - %s",
										last_error_code, last_error_msg.c_str()
					);
				}

                if ( EAGAIN == last_error_code ) {
					break;
                } else if ( 0 == last_error_code ) { // no messages are available to be received and the peer has performed an orderly shutdown
                    // TODO CONNECTORS throw ev::Exception("No messages are available to be received and the peer has performed an orderly shutdown!");
                    break;
                } else {
                    throw ev::Exception("Unable to read data from socket : %d - %s!",
                                        last_error_code,
                                        handler.socket_.GetLastReceiveErrorString().c_str()
                    );
                }
            }

            // ... stats ...
            rx_count     += static_cast<int>(handler.buffer_bytes_count_);
            msg_received += 1;
            
            // ... decode message ...
            const std::string message = std::string(reinterpret_cast<const char* const>(handler.buffer_), handler.buffer_bytes_count_);
            OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                                "sh: received " SIZET_FMT " byte(s) - %s",
                                handler.buffer_bytes_count_, message.c_str()
            );

            void* memory = nullptr;
            if ( 1 != sscanf(message.c_str(), "callback:%p", &memory) ) {
                // ... fatal error ...
                throw ev::Exception("Unable to read callback addr from socket message!");
            }
            
            // ... grab callback ...
            ev::ngx::Bridge::Callback* callback = static_cast<ev::ngx::Bridge::Callback*>(memory);
            
            callbacks_remaining = ( std::atomic_fetch_add(&handler.pending_callbacks_count_, -1) - 1 );
            
            // ... perform callback ...
            callback->Call();
            OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                                "smt: ~> callback %p performed",
                                callback
            );
            // ... forget it ...
            delete callback;
        }
        
        OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                            "sh: received %d message(s) [ %d byte(s) ], pending %d callbacks(s)",
                            msg_received, rx_count, callbacks_remaining
        );
        (void)callbacks_remaining;

    } catch (const ev::Exception& a_ev_exception) {
        OSALITE_BACKTRACE();
        handler.ThrowFatalException(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        OSALITE_BACKTRACE();
        handler.ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        OSALITE_BACKTRACE();
        handler.ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        OSALITE_BACKTRACE();
        handler.ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        OSALITE_BACKTRACE();
        handler.ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
}

/**
 * @brief Handler called by the ngx event loop.
 *
 * @param a_event
 */
void ev::ngx::Bridge::DifferedHandler (ngx_event_t* a_event)
{
    ev::ngx::Bridge& handler = ev::ngx::Bridge::GetInstance();

    // ... grab callback ...
    ev::ngx::Bridge::Callback* callback = static_cast<ev::ngx::Bridge::Callback*>(a_event->data);

    const int callbacks_remaining = ( std::atomic_fetch_add(&handler.pending_callbacks_count_, -1) - 1 );
    
    // .. calculate elapsed time ...
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - callback->start_time_point_).count();

    OSALITE_DEBUG_TRACE("ev_ngx_shared_handler",
                        "dh: performed callback after %d ms [ " INT64_FMT " ], pending %d callbacks(s)",
                        (int)elapsed, callback->timeout_ms_, callbacks_remaining
    );
    (void)callbacks_remaining;
    (void)elapsed;

    // ... perform callback ...
    callback->Call();
    
    // ... forget it ...
    delete callback;
}

/**
 * @brief Dummy.
 *
 * @param a_connection
 * @param a_buffer
 * @param a_size
 */
ssize_t ev::ngx::Bridge::Receive (ngx_connection_t* /* a_connection */, u_char* /* a_buffer */, size_t /* a_size */)
{
    OSALITE_ASSERT(1==1);
    return 0;
}

/**
 * @brief Dummy.
 *
 * @param a_connection
 * @param a_buffer
 * @param a_size
 */
ssize_t ev::ngx::Bridge::Send (ngx_connection_t* /* a_connection */, u_char* /* a_buffer */, size_t /* a_size */)
{
    OSALITE_ASSERT(1==1);
    return 0;
}
