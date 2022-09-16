/**
 * @file event.cc
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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

#include "cc/ngx/event.h"

#include "cc/macros.h"
#include "cc/debug/types.h"
#include "cc/types.h"

#include "cc/ngx/registry.h"


/**
 * @brief Default constructor.
 */
cc::ngx::Event::Event ()
{
    connection_         = nullptr;
    event_              = nullptr;
    log_                = nullptr;
    buffer_             = nullptr;
    buffer_length_      = 0;
    buffer_bytes_count_ = 0;
    pending_callbacks_count_ = 0;
}

/**
 * @brief Destructor.
 */
cc::ngx::Event::~Event ()
{
    Unregister();
}

// MARK: -

/**
 * @brief Register this event @ NGX event loop.
 *
 * @param a_socket_fn Socket file URI.
 * @param a_callback  Function to call on fatal exception.
 */
void cc::ngx::Event::Register (const std::string& a_socket_fn, Event::FatalExceptionCallback a_callback)
{
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                       "~> %s(...)",
                       __FUNCTION__
    );
    
    // ... ngx sanity check ...
    if ( nullptr == ngx_cycle ) {
        // ... ngx not ready ...
        throw cc::Exception("Invalid startup call - ngx_cycle not set!");
    }
    // ... sanity check ...
    if ( nullptr != connection_ || nullptr != event_ || nullptr != log_ ) {
        // ... insane call ...
        throw cc::Exception("Already initialized!");
    }
    
    //
    // BUFFER
    //
    
    // ... allocate a new buffer ...
    buffer_        = new uint8_t[1024];
    buffer_length_ = 1024;
    
    //
    // TRACKER
    //
    
    pending_callbacks_count_ = 0;
    
    //
    // SOCKET
    //
    
    // ... create socket ...
    if ( false == socket_.Create(a_socket_fn) ) {
        throw cc::Exception("Can't open a socket, using '%s' file: %s!", a_socket_fn.c_str(), socket_.GetLastConfigErrorString().c_str());
    }

    // ... 'this' side socket must be binded now ...
    if ( false == socket_.Bind() ) {
      // ... unable to bind socket ...
      throw cc::Exception("Unable to bind client: %s", socket_.GetLastConfigErrorString().c_str());
    }

    // ... set non-block ...
    if ( false == socket_.SetNonBlock() ) {
      throw cc::Exception("Unable to set socket non-block property:  %s", socket_.GetLastConfigErrorString().c_str());
    }
    
    //
    // LOG
    //
    log_ = (ngx_log_t*)malloc(sizeof(ngx_log_t));
    if ( nullptr == log_ ) {
        throw cc::Exception("Unable to create 'shared handler' log!");
    }
    ngx_memzero(log_, sizeof(ngx_log_t));
    
    //
    // CONNECTION
    //
    
    // ... create a new connection ...
    connection_ = ngx_get_connection((ngx_socket_t)socket_.GetFileDescriptor(), log_);
    if ( nullptr == connection_ ) {
        throw cc::Exception("Unable to create 'shared handler' connection!\n");
    }
    connection_->write->log = log_;
    connection_->read->log  = log_;
    connection_->recv       = cc::ngx::Event::Receive;
    connection_->send       = cc::ngx::Event::Send;
    
    //
    // EVENT
    //
    
    // ... create a new connection ...
    event_ = (ngx_event_t*)malloc(sizeof(ngx_event_t));
    if ( NULL == event_ ) {
        throw cc::Exception("Unable to create 'shared handler' event!\n");
    }
    // ... just keeping the same behavior as in ngx_pcalloc ...
    ngx_memzero(event_, sizeof(ngx_event_t));
    
    /* UDP sockets are always ready to write */
    event_->ready   = 1;
    
    event_->log     = log_;
    event_->handler = cc::ngx::Event::Handler;
    event_->data    = connection_;

    // ... register ...
    cc::ngx::Registry::GetInstance().Register(event_, this);

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
        throw cc::Exception("Unable to add 'shared handler' event: %ld!\n", ngx_add_rv);
    }
    
    // ... done ...
    fatal_exception_callback_ = a_callback;
    
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                       "<~ %s(...) - connection_=%p, event_=%p, socket[ %d] %s",
                       __FUNCTION__, (void*)connection_, (void*)event_,
                       socket_.GetFileDescriptor(), a_socket_fn.c_str()
    );
}

/**
 * @brief Unregister this event from NGX event loop.
 */
void cc::ngx::Event::Unregister ()
{
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event", "~> %s()", __FUNCTION__);
    // ... clean up ...
    if ( nullptr != event_ ) {
        // ... unregister ...
        cc::ngx::Registry::GetInstance().Unregister(event_);
        // ... delete ...
        ngx_del_event(event_, NGX_READ_EVENT, 0);
        // .. release
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
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event", "<~ %s()", __FUNCTION__);
}

// MARK: -

/**
 * @brief Call this method to perform a callback on main thread.
 *
 * @param a_callback Function to call.
 * @param a_payload  Payload to pass to callback.
 * @param a_timeout  Delay in milliseconds.
 */
void cc::ngx::Event::CallOnMainThread (std::function<void(void* a_payload)> a_callback, void* a_payload, uint64_t a_timeout_ms)
{
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);

    ScheduleCalbackOnMainThread(new cc::ngx::Event::Callback(this, a_callback, a_payload, a_timeout_ms), a_timeout_ms);
}

/**
 * @brief Call this method to perform a callback on main thread.
 *
 * @param a_callback Function to call.
 * @param a_timeout  Delay in milliseconds.
 */
void cc::ngx::Event::CallOnMainThread (std::function<void()> a_callback, uint64_t a_timeout_ms)
{
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);

    ScheduleCalbackOnMainThread(new cc::ngx::Event::Callback(this, a_callback, a_timeout_ms), a_timeout_ms);
}

// MARK: -

/**
 * @brief Schedule a callback on 'main' thread.
 *
 * @param a_callback Function to call.
 * @param a_timeout  Delay in milliseconds.
 */
void cc::ngx::Event::ScheduleCalbackOnMainThread (cc::ngx::Event::Callback* a_callback, uint64_t a_timeout_ms)
{
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                       "smt: ~> scheduling callback %p...",
                       (void*)a_callback
    );
    
    if ( 0 == a_timeout_ms ) {
        // ... keep track of # of pending callbacks ...
        const int remaining = std::atomic_fetch_add(&pending_callbacks_count_,1);
        // ... send message through socket to be read at 'main' thread ...
        if ( false == socket_.Send("callback:%p", static_cast<void*>(a_callback)) ) {
            if ( EAGAIN == socket_.GetLastSendError() ) {
                // ... for debug purposes
                CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                                   "smt: ~> (re)scheduling callback %p...",
                                   (void*)a_callback
                );
                // ... deferred ...
                ScheduleCalbackOnMainThread(a_callback, 1000);
                return;
            } else {
                // ... clean up ..
                delete a_callback;
                // ... notify ...
                throw cc::Exception("Unable to send a message through socket: %s!",
                                    socket_.GetLastSendErrorString().c_str()
                );
            }
        }
        // ... for debug purposes
        CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                           "smt: ~> callback %p scheduled [ pending_callbacks_count_ = %d ]",
                           (void*)a_callback, remaining
        );
        (void)remaining;
    } else {
        // ... differed ...
        a_callback->ngx_event_ = (ngx_event_t*)malloc(sizeof(ngx_event_t));
        if ( NULL == a_callback->ngx_event_ ) {
            // ... clean up ..
            delete a_callback;
            // ... notify ...
            throw cc::Exception("Unable to create 'shared handler' differed event!\n");
        }
        // ... just keeping the same behavior as in ngx_pcalloc ...
        ngx_memzero(a_callback->ngx_event_, sizeof(ngx_event_t));
        // ... set event ...
        a_callback->ngx_event_->log     = log_;
        a_callback->ngx_event_->handler = cc::ngx::Event::DifferedHandler;
        a_callback->ngx_event_->data    = a_callback;
        // ... keep track of # of pending callbacks ...
        const int remaining = std::atomic_fetch_add(&pending_callbacks_count_,1);
        // ... schedule a callback through socket to be called at 'main' thread ...
        ngx_add_timer(a_callback->ngx_event_, (ngx_msec_t)(a_timeout_ms));
        // ... for debug purposes
        CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                           "smt: ~> callback %p scheduled [ pending_callbacks_count_ = %d ], timeout in " INT64_FMT "ms",
                           (void*)a_callback, remaining, a_timeout_ms
        );
        (void)remaining;
    }
}

/**
 * @brief Call this method handle with a fatal exception.
 *
 *@param a_exception Catched exception.
 */
void cc::ngx::Event::ThrowFatalException (const cc::Exception& a_exception)
{
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event", "~> %s()", __FUNCTION__);
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);
    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event", "<~ %s()", __FUNCTION__);
    // ... notify ...
    fatal_exception_callback_(a_exception);
}

// MARK: -

/**
 * @brief Handler called by the ngx event loop.
 *
 * @param a_event NGX event to process.
 */
void cc::ngx::Event::Handler (ngx_event_t* a_event)
{
    const void* data = cc::ngx::Registry::GetInstance().Data(a_event);
    CC_ASSERT(nullptr != data);
    
    // ... grab callback ...
    cc::ngx::Event* self    = const_cast<cc::ngx::Event*>(static_cast<const cc::ngx::Event*>(data));
    cc::ngx::Event& handler = *self;

    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                       "sh: a_event->available=%d, a_event->ready=%d, connection_[%p]->fd=%d",
                       a_event->available, a_event->ready,
                       (void*)handler.connection_,
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
                const int last_error_code = handler.socket_.GetLastReceiveError();
                // ... for debug purposes
                CC_DEBUG_LOG_IF_REGISTERED_RUN("cc::ngx::Event", {
                    const std::string last_error_msg  = handler.socket_.GetLastReceiveErrorString();
                    CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                                       "sh: rx error %d - %s",
                                       last_error_code, last_error_msg.c_str()
                    );
                })

                if ( EAGAIN == last_error_code ) {
                    break;
                } else if ( 0 == last_error_code ) { // no messages are available to be received and the peer has performed an orderly shutdown
                    // TODO CONNECTORS throw cc::Exception("No messages are available to be received and the peer has performed an orderly shutdown!");
                    break;
                } else {
                    throw ::cc::Exception("Unable to read data from socket : %d - %s!",
                                          last_error_code, handler.socket_.GetLastReceiveErrorString().c_str()
                    );
                }
            }

            // ... stats ...
            rx_count     += static_cast<int>(handler.buffer_bytes_count_);
            msg_received += 1;

            // ... decode message ...
            const std::string message = std::string(reinterpret_cast<const char* const>(handler.buffer_), handler.buffer_bytes_count_);
            // ... for debug purposes
            CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                               "sh: received " SIZET_FMT " byte(s) - %s",
                               handler.buffer_bytes_count_, message.c_str()
            );

            void* memory = nullptr;
            if ( 1 != sscanf(message.c_str(), "callback:%p", &memory) ) {
                // ... fatal error ...
                throw ::cc::Exception("%s", "Unable to read callback addr from socket message!");
            }

            // ... grab callback ...
            cc::ngx::Event::Callback* callback = static_cast<cc::ngx::Event::Callback*>(memory);

            callbacks_remaining = ( std::atomic_fetch_add(&handler.pending_callbacks_count_, -1) - 1 );

            // ... perform callback ...
            callback->Call();
            // ... for debug purposes
            CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                               "smt: ~> callback %p performed",
                               (void*)callback
            );
            // ... forget it ...
            delete callback;
        }

        // ... for debug purposes
        CC_DEBUG_LOG_TRACE("cc::ngx::Event",
                           "sh: received %d message(s) [ %d byte(s) ], pending %d callbacks(s)",
                           msg_received, rx_count, callbacks_remaining
        );
        (void)callbacks_remaining;

    } catch (const cc::Exception& a_exception) {
        handler.ThrowFatalException(a_exception);
    } catch (...) {
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::Exception& a_cc_exception) {
            handler.ThrowFatalException(a_cc_exception);
        } catch (...) {
            handler.ThrowFatalException(::cc::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
        }
    }
}

/**
 * @brief Handler called by the ngx event loop.
 *
 * @param a_event NGX event to process.
 */
void cc::ngx::Event::DifferedHandler (ngx_event_t* a_event)
{
    // ... grab callback ...
    cc::ngx::Event::Callback* callback = static_cast<cc::ngx::Event::Callback*>(a_event->data);
    cc::ngx::Event& handler = *const_cast<cc::ngx::Event*>(callback->event());

    const int callbacks_remaining = ( std::atomic_fetch_add(&handler.pending_callbacks_count_, -1) - 1 );
    
    // .. calculate elapsed time ...
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - callback->start_time_point_).count();

    // ... for debug purposes
    CC_DEBUG_LOG_TRACE("cc::ngx::Event",
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

// MARK: -

/**
 * @brief Dummy.
 *
 * @param a_connection NGX connection.
 * @param a_buffer     Buffer to read.
 * @param a_size       Amount of elements to read from buffer.
 */
ssize_t cc::ngx::Event::Receive (ngx_connection_t* /* a_connection */, u_char* /* a_buffer */, size_t /* a_size */)
{
    CC_ASSERT(1==1);
    return 0;
}

/**
 * @brief Dummy.
 *
 * @param a_connection NGX connection.
 * @param a_buffer     Buffer to write to.
 * @param a_size       Number of  allowedelements to write to buffer from buffer.
 */
ssize_t cc::ngx::Event::Send (ngx_connection_t* /* a_connection */, u_char* /* a_buffer */, size_t /* a_size */)
{
    CC_ASSERT(1==1);
    return 0;
}
