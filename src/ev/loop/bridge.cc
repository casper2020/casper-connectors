/**
 * @file main.cc
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

#include "ev/loop/bridge.h"

#include "ev/exception.h"

#include "osal/osalite.h"

#include <mutex> // std::mutex

/**
 * @brief Default constructor.
 */
ev::loop::Bridge::Bridge ()
{
#if 0 // TODO
    thread_                  = nullptr;
#endif
    thread_id_               = osal::ThreadHelper::k_invalid_thread_id_;
    aborted_                 = false;
    running_                 = false;
    event_base_              = nullptr;
    hack_event_              = nullptr;
    watchdog_event_          = nullptr;
    socket_event_            = nullptr;
    pending_callbacks_count_ = 0;
    rx_buffer_               = new uint8_t[1024];
    rx_buffer_length_        = 1024;
    rx_buffer_bytes_count_   = 0;
    event_set_fatal_callback(ev::loop::Bridge::EventFatalCallback);
#ifdef DEBUG
    event_set_log_callback(ev::loop::Bridge::EventLogCallback);
#endif
}

/**
 * @brief Destructor.
 */
ev::loop::Bridge::~Bridge ()
{
    Stop(-1);
}

/**
 * @brief Synchronously start this event loop.
 *
 * @param a_socket_fn
 * @param a_fatal_exception_callback
 *
 * @return A callback to be used when it's necessary run code in the 'main' thread.
 */
ev::loop::Bridge::CallOnMainThreadCallback ev::loop::Bridge::Start (const std::string& a_socket_fn,
                                                                    ev::loop::Bridge::FatalExceptionCallback a_fatal_exception_callback)
{
    try {
        
#if 0 // TODO
        if ( nullptr != thread_ ) {
            throw ev::Exception("Unable to start main loop: alread running!");
        }
#endif
        
        if ( nullptr == event_base_ ) {
            event_base_ = event_base_new();
            if ( nullptr == event_base_ ) {
                throw ev::Exception("Unable to start hub loop: can't create 'base' event!");
            }
        }
        
        timeval tv;
        tv.tv_sec  = 365 * 24 * 3600;
        tv.tv_usec = 0;
        
        if ( nullptr != hack_event_ ) {
            event_del(hack_event_);
            event_free(hack_event_);
        }
        hack_event_ = evtimer_new(event_base_, ev::loop::Bridge::LoopHackEventCallback, &hack_event_);
        if ( nullptr == hack_event_ ) {
            throw ev::Exception("Unable to start hub loop - can't create 'hack' event!");
        }
        const int rv = evtimer_add(hack_event_, &tv);
        if ( rv < 0 ) {
            throw ev::Exception("Unable to start hub loop: can't add 'hack' event - error code %d !",
                                rv);
        }
        
        if ( nullptr != watchdog_event_ ) {
            event_del(watchdog_event_);
            event_free(watchdog_event_);
        }
        watchdog_event_ = event_new(event_base_, -1, EV_PERSIST, ev::loop::Bridge::WatchdogCallback, this);
        if ( nullptr == watchdog_event_ ) {
            throw ev::Exception("Unable to start hub loop - can't create 'watchdog' event!");
        }
        const int wd_rv = event_add(watchdog_event_, &tv);
        if ( wd_rv < 0 ) {
            throw ev::Exception("Unable to start hub loop: can't add 'watchdog' event - error code %d !",
                                wd_rv);
        }
        
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
        
        if ( nullptr != socket_event_ ) {
            event_del(socket_event_);
            event_free(socket_event_);
        }
        socket_event_ = event_new(event_base_, socket_.GetFileDescriptor(), EV_READ, ev::loop::Bridge::SocketCallback, this);
        if ( nullptr == socket_event_ ) {
            throw ev::Exception("Unable to start hub loop - can't create 'socket' event!");
        }
        const int sk_rv = event_add(socket_event_, nullptr);
        if ( sk_rv < 0 ) {
            throw ev::Exception("Unable to start hub loop: can't add 'socket' event - error code %d !",
                                sk_rv);
        }
        
        // ... keep track of callbacks ...
        fatal_exception_callback_ = a_fatal_exception_callback;
        call_on_main_thread_      = [this] (const std::function<void()>& a_callback) {
            CallOnMainThread(a_callback);
        };
        
        // TODO NOW
#if 0
        thread_ = new std::thread(&ev::loop::Bridge::Loop, this);
        thread_->detach();
        
        detach_condition_.Wait();
#endif

    } catch (const ev::Exception& a_ev_exception) {
        a_fatal_exception_callback(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        a_fatal_exception_callback(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        a_fatal_exception_callback(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        a_fatal_exception_callback(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        a_fatal_exception_callback(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
    
    return call_on_main_thread_;
}

/**
 * @brief Stop this hub.
 *
 * @param a_sig_no
 */
void ev::loop::Bridge::Stop (int a_sig_no)
{
    OSALITE_DEBUG_TRACE("ev_bridge", "~> Stop(a_sig_no=%d)...", a_sig_no);
    
    aborted_ = true;
    
    if ( nullptr != hack_event_ ) {
        event_del(hack_event_);
        event_free(hack_event_);
        hack_event_ = nullptr;
    }
    
    if ( nullptr != watchdog_event_ ) {
        event_del(watchdog_event_);
        event_free(watchdog_event_);
        watchdog_event_ = nullptr;
    }
    
    if ( nullptr != socket_event_ ) {
        event_del(socket_event_);
        event_free(socket_event_);
        socket_event_ = nullptr;
    }
    
    if ( nullptr != event_base_ ) {
        event_base_loopbreak(event_base_);
        event_base_free(event_base_);
        event_base_ = nullptr;
        // ... should wait until loop breaks?
        if ( -1 == a_sig_no && true == running_ ) {
            //
            // SIGKILL || SIGTERM
            //
            // - THEADS WILL STOP RUNNING
            // - NO POINT IN WAITING FOR A THREAD THAT WONT RUN!
            //
            OSALITE_DEBUG_TRACE("ev_bridge", "~> stop_cv_ locked from 'main' thread loop...");
            abort_condition_.Wait();
        }
    }

#if 0 // TODO
    if ( nullptr != thread_ ) {
        delete thread_;
        thread_ = nullptr;
    }
#endif
    
    socket_.Close();
    
    fatal_exception_callback_ = nullptr;
    
    if ( nullptr != rx_buffer_ ) {
        delete [] rx_buffer_;
        rx_buffer_             = nullptr;
        rx_buffer_length_      = 0;
        rx_buffer_bytes_count_ = 0;
    }

    OSALITE_DEBUG_TRACE("ev_bridge", "<~ Stop()...");
}

/**
 * @brief Quit bridge loop.
 */
void ev::loop::Bridge::Quit ()
{
    if ( true == running_ && false == aborted_ ) {
        aborted_ = true;
        if ( nullptr != event_base_ ) {
	  event_active(watchdog_event_, EV_TIMEOUT, 0);
	  abort_condition_.Wait();
        }
    }
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
void ev::loop::Bridge::CallOnMainThread (std::function<void(void* a_payload)> a_callback, void* a_payload, int64_t a_timeout_ms)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);
    
    ScheduleCalbackOnMainThread(new ev::loop::Bridge::Callback(a_callback, a_payload, a_timeout_ms), a_timeout_ms);
}

/**
 * @brief Call this method to perform a callback on main thread.
 *
 * @param a_callback
 * @param a_timeout
 */
void ev::loop::Bridge::CallOnMainThread (std::function<void()> a_callback, int64_t a_timeout_ms)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);
    
    ScheduleCalbackOnMainThread(new ev::loop::Bridge::Callback(a_callback, a_timeout_ms), a_timeout_ms);
}

/**
 * @brief Call this method handle with a fatal exception.
 */
void ev::loop::Bridge::ThrowFatalException (const ev::Exception& a_ev_exception)
{
    static std::mutex ___mutex;
    std::lock_guard<std::mutex> lock(___mutex);
    
    fatal_exception_callback_(a_ev_exception);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Event loop.
 */
void ev::loop::Bridge::Loop ()
{
    thread_id_ = osal::ThreadHelper::GetInstance().CurrentThreadID();
    
    running_ = true;

    if ( false == osal::ThreadHelper::GetInstance().AtMainThread() ) {
      osal::posix::ThreadHelper::BlockSignals({SIGTTIN, SIGTERM, SIGQUIT});
    }

#if 0 // TODO
    detach_condition_.Wake();
#endif
    
#define EVLOOP_NO_EXIT_ON_EMPTY 0x04
    
    while ( false == aborted_ && nullptr != event_base_ ) {
        
        int rv = event_base_loop(event_base_, EVLOOP_NO_EXIT_ON_EMPTY);
        OSALITE_DEBUG_TRACE("ev_bridge", "~> event_base_loop=%d!", rv);
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

    running_ = false;
    
    abort_condition_.Wake();
}

/**
 * @brief Schedule a callback on 'main' thread.
 *
 * @param a_callback
 * @param a_timeout_ms
 */
void ev::loop::Bridge::ScheduleCalbackOnMainThread (ev::loop::Bridge::Callback* a_callback, int64_t a_timeout_ms)
{
    OSALITE_DEBUG_TRACE("ev_bridge_handler",
                        "smt: ~> scheduling callback %p...",
                        a_callback
    );
    
    if ( 0 == a_timeout_ms ) {
        // ... keep track of # of pending callbacks ...
        const int remaining = std::atomic_fetch_add(&pending_callbacks_count_,1);
        // ... send message through socket to be read at 'main' thread ...
        if ( false == socket_.Send("callback:%p", static_cast<void*>(a_callback)) ) {
            if ( EAGAIN == socket_.GetLastSendError() ) {
                OSALITE_DEBUG_TRACE("ev_bridge_handler",
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
        OSALITE_DEBUG_TRACE("ev_bridge_handler",
                            "smt: ~> callback %p scheduled [ pending_callbacks_count_ = %d ]",
                            a_callback, remaining
        );
        (void)remaining;
    } else {
        
        // ... keep track of # of pending callbacks ...
        const int remaining = std::atomic_fetch_add(&pending_callbacks_count_,1);
        
        // ... schedule a callback through socket to be called at 'main' thread ...
        struct timeval time;
        time.tv_sec  = ( a_timeout_ms / 1000 );
        time.tv_usec = ( ( a_timeout_ms % 1000 ) * 1000 );
        
        a_callback->event_ = evtimer_new(event_base_, ev::loop::Bridge::DifferedScheduleCallback, a_callback);
        if ( nullptr == a_callback->event_ ) {
            throw ev::Exception("Unable schedule callback on main thread - can't create 'differed' event!");
        }
        const int rv = evtimer_add(a_callback->event_, &time);
        if ( rv < 0 ) {
            throw ev::Exception("Unable schedule callback on main thread - can't add 'differed' event - error code %d !",
                                rv
            );
        }
        a_callback->parent_ptr_ = this;
        
        OSALITE_DEBUG_TRACE("ev_bridge_handler",
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
 * @brief A function to be called if Libevent encounters a fatal internal error.
 *
 * @param a_error
 */
void ev::loop::Bridge::EventFatalCallback (int a_error)
{
    for ( auto output : { stderr, stdout } ) {
        fprintf(output, "Event loop fatal error - code %d\n", a_error);
        fflush(output);
    }
    exit(-1);
}

/**
 * @brief A callback function used to intercept Libevent's log messages.
 *
 * @param a_severity
 * @param a_msg
 */
void ev::loop::Bridge::EventLogCallback (int a_severity, const char* a_msg)
{
    fprintf(stderr, "Log: [%8d] %s\n", a_severity, a_msg);
    fflush(stderr);
}


/**
 * @brief Handle socket read / write events.
 *
 * @param a_df
 * @param a_flags
 * @param a_arg
 */
void ev::loop::Bridge::SocketCallback (evutil_socket_t /* a_fd */, short a_flags, void* a_arg)
{
    ev::loop::Bridge* self = (ev::loop::Bridge*)a_arg;
    
    // .... we're only expecting read event ...
    if ( EV_READ != ( a_flags & EV_READ ) ) {
        return;
    }
    
    // ... read and process all available messages ...
    try {
        
        int msg_received  = 0;
        int rx_count      = 0;
        int callbacks_remaining = 0;
        
        while ( true == self->socket_.Receive(self->rx_buffer_, self->rx_buffer_length_, self->rx_buffer_bytes_count_) ) {
            
            // ... stats ...
            rx_count     += static_cast<int>(self->rx_buffer_bytes_count_);
            msg_received += 1;
            
            // ... decode message ...
            const std::string message = std::string(reinterpret_cast<const char* const>(self->rx_buffer_), self->rx_buffer_bytes_count_);
            OSALITE_DEBUG_TRACE("ev_bridge",
                                "sh: received " SIZET_FMT " byte(s) - %s",
                                self->rx_buffer_bytes_count_, message.c_str()
            );
            
            void* memory = nullptr;
            if ( 1 != sscanf(message.c_str(), "callback:%p", &memory) ) {
                // ... fatal error ...
                throw ev::Exception("Unable to read callback addr from socket message!");
            }
            
            // ... grab callback ...
            ev::loop::Bridge::Callback* callback = static_cast<ev::loop::Bridge::Callback*>(memory);
            
            callbacks_remaining = ( std::atomic_fetch_add(&self->pending_callbacks_count_, -1) - 1 );
            
            // ... perform callback ...
            callback->Call();
            OSALITE_DEBUG_TRACE("ev_bridge",
                                "smt: ~> callback %p performed",
                                callback
            );
            // ... forget it ...
            delete callback;
            
        }
        
        OSALITE_DEBUG_TRACE("ev_bridge",
                            "sh: received %d message(s) [ %d byte(s) ], pending %d callbacks(s)",
                            msg_received, rx_count, callbacks_remaining
        );
        (void)callbacks_remaining;
                
        // ... fatal error?
        const int last_error_code = self->socket_.GetLastReceiveError();
        OSALITE_DEBUG_IF("ev_bridge") {
            const std::string last_error_msg  = self->socket_.GetLastReceiveErrorString();
            OSALITE_DEBUG_TRACE("ev_bridge",
                                "sh: rx error %d - %s",
                                last_error_code, last_error_msg.c_str()
                                );
        }
        
        if ( EAGAIN == last_error_code ) {
            // ... pass ...
        } else if ( 0 == last_error_code ) { // no messages are available to be received and the peer has performed an orderly shutdown
            // TODO CONNECTORS throw ev::Exception("No messages are available to be received and the peer has performed an orderly shutdown!");
            // ... pass ...
        } else {
            throw ev::Exception("Unable to read data from socket : %d - %s!",
                                last_error_code,
                                self->socket_.GetLastReceiveErrorString().c_str()
            );
        }
        
        // ... renew read intent ...
        if ( nullptr != self->socket_event_ ) {
            const int del_rc = event_del(self->socket_event_);
            if ( 0 != del_rc ) {
                throw ev::Exception("Error while deleting socket event event: code %d!", del_rc);
            }
            const int assign_rv = event_assign(self->socket_event_, self->event_base_, self->socket_.GetFileDescriptor(), EV_READ, ev::loop::Bridge::SocketCallback, self);
            if ( 0 != assign_rv ) {
                throw ev::Exception("Error while assigning socket event: code %d!", assign_rv);
            }
            const int add_rv = event_add(self->socket_event_, nullptr);
            if ( 0 != add_rv ) {
                throw ev::Exception("Error while adding socket event: code %d!", add_rv);
            }
        }
        
    } catch (const ev::Exception& a_ev_exception) {
        self->ThrowFatalException(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        self->ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        self->ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {        ;
        self->ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        self->ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
}

/**
 * @brief This is a hack to prevent event_base_loop from exiting;
 *         The flag EVLOOP_NO_EXIT_ON_EMPTY is somehow ignored, at least on Mac OS X.
 *
 * @param a_fd    A file descriptor or signal.
 * @param a_flags One or mode EV_* flags.
 * @param a_arg   User-supplied argument.
 */
void ev::loop::Bridge::LoopHackEventCallback (evutil_socket_t /* a_fd */, short /* a_flags */, void* a_arg)
{
    timeval tv;
    tv.tv_sec  = 365 * 24 * 3600;
    tv.tv_usec = 0;
    event *ev = *(event **)a_arg;
    (void)evtimer_add(ev, &tv);
}

/**
 * @brief Handle event to break base.
 *
 * @param a_fd
 * @param a_flags
 * @param a_arg
 */
void ev::loop::Bridge::WatchdogCallback (evutil_socket_t /* a_fd */, short /* a_flags */, void* a_arg)
{
    ev::loop::Bridge* self = (ev::loop::Bridge*)a_arg;
    
    OSALITE_DEBUG_TRACE("ev_bridge", "~> Watchdog reporting for duty...");
    
    if ( true == self->aborted_ ) {
        OSALITE_DEBUG_TRACE("ev_bridge", "~> Watchdog decision is... break it!");
        event_base_loopbreak(self->event_base_);
    } else {
        OSALITE_DEBUG_TRACE("ev_bridge", "~> Watchdog decision is... let it lingering!");
        timeval tv;
        tv.tv_sec  = 365 * 24 * 3600;
        tv.tv_usec = 0;
        event *ev = *(event **)self->watchdog_event_;
        (void)evtimer_add(ev, &tv);
    }
}

/**
 * @brief Handle socket read / write events.
 *
 * @param a_fd
 * @param a_flags
 * @param a_arg
 */
void ev::loop::Bridge::DifferedScheduleCallback (evutil_socket_t /* a_fd */, short a_flags, void* a_arg)
{
    ev::loop::Bridge::Callback* callback = (ev::loop::Bridge::Callback*)a_arg;
    ev::loop::Bridge*           self     = (ev::loop::Bridge*)callback->parent_ptr_;
    
    self->ScheduleCalbackOnMainThread(callback, /* a_timeout_ms */ 0);
}
