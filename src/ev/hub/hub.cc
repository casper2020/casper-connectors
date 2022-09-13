/**
 * @file hub.c
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

#include "ev/hub/hub.h"

#include "ev/exception.h"

#include "osal/osalite.h"

#include "cc/threading/worker.h"

#include <sstream> // std::stringstream

#ifdef __APPLE__
#pragma mark - PublishCallback
#endif

/**
 * @brief Default Constructor
 */
ev::hub::Hub::PublishCallback::PublishCallback (ev::Bridge& a_bridge, ev::hub::PublishStepCallback& a_callback)
    : ev::hub::PublishCallback(a_callback), bridge_(a_bridge)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::hub::Hub::PublishCallback::~PublishCallback ()
{
    /* empty */
}

/**
 * @brief This function will be called to perform two callbacks, on in this 'Hub' thread another in the 'main' thread.
 *
 * @param a_background
 * @param a_foreground
 */
void ev::hub::Hub::PublishCallback::Call (std::function<void*()> a_background,
                                          std::function<void(void* /* a_payload */, ev::hub::PublishStepCallback /* a_publish_step_callback */)> a_foreground)
{
    bridge_.CallOnMainThread(
                             [this, a_foreground](void* a_payload) {
                                 CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                                 a_foreground(a_payload, callback_);
                             },
                             a_background()
    );
}

#ifdef __APPLE__
#pragma mark - NextCallback
#endif

/**
 * @brief Default Constructor
 */
ev::hub::Hub::NextCallback::NextCallback (Bridge& a_bridge, ev::hub::NextStepCallback& a_callback)
    : ev::hub::NextCallback(a_callback), bridge_(a_bridge)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::hub::Hub::NextCallback::~NextCallback ()
{
    /* empty */
}

/**
 * @brief This function will be called to perform two callbacks, on in this 'Hub' thread another in the 'main' thread.
 *
 * @param a_background
 * @param a_foreground
 */
void ev::hub::Hub::NextCallback::Call (std::function<void*()> a_background,
                                  std::function<void(void* /* a_payload */, ev::hub::NextStepCallback /* a_publish_step_callback */)> a_foreground)
{
    bridge_.CallOnMainThread(
                             [this, a_foreground](void* a_payload) {
                                 CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                                 a_foreground(a_payload, callback_);
                             },
                             a_background()
    );
}

#ifdef __APPLE__
#pragma mark - DisconnectedCallback
#endif

/**
 * @brief Default Constructor
 */
ev::hub::Hub::DisconnectedCallback::DisconnectedCallback (ev::Bridge& a_bridge, ev::hub::DisconnectedStepCallback& a_callback)
    : ev::hub::DisconnectedCallback(a_callback), bridge_(a_bridge)
{
    /* empty */
}

/**
 * @brief Destructor.
 */
ev::hub::Hub::DisconnectedCallback::~DisconnectedCallback ()
{
    /* empty */
}

/**
 * @brief This function will be called to perform two callbacks, on in this 'Hub' thread another in the 'main' thread.
 *
 * @param a_background
 * @param a_foreground
 */
void ev::hub::Hub::DisconnectedCallback::Call (std::function<void*()> a_background,
                                               std::function<void(void* /* a_payload */, ev::hub::DisconnectedStepCallback /* a_publish_step_callback */)> a_foreground)
{
    bridge_.CallOnMainThread(
                             [this, a_foreground](void* a_payload) {
                                 CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                                 a_foreground(a_payload, callback_);
                             },
                             a_background()
    );
}

#ifdef __APPLE__
#pragma mark - Hub
#endif

//      19    :  3   :   3    :  3
// <invoke_id>:<mode>:<target>:<tag>
const char* const ev::hub::Hub::k_msg_no_payload_format_   = INT64_FMT_ZP(19) ":" UINT8_FMT_ZP(3) ":" UINT8_FMT_ZP(3) ":" UINT8_FMT_ZP(3);
// <invoke_id>:<mode>:<target>:<tag>:<obj_addr>
const char* const ev::hub::Hub::k_msg_with_payload_format_ = INT64_FMT_ZP(19) ":" UINT8_FMT_ZP(3) ":" UINT8_FMT_ZP(3) ":" UINT8_FMT_ZP(3) ":%p";
const size_t      ev::hub::Hub::k_msg_min_length_          = 31;

const int64_t     ev::hub::Hub::k_wake_msg_invalid_id_     = std::numeric_limits<int64_t>::min();

/**
 * @brief Default constructor.
 *
 * @param a_socket_file_name
 * @param a_bridge
 */
ev::hub::Hub::Hub (const std::string& a_name,
                   Bridge& a_bridge, const std::string& a_socket_file_name, std::atomic<int>& a_pending_callbacks_count)
    : name_(a_name), bridge_(a_bridge), thread_(nullptr), configured_(false), running_(false), aborted_(false), pending_callbacks_count_(a_pending_callbacks_count)
{
    event_base_                  = nullptr;
    hack_event_                  = nullptr;
    watchdog_event_              = nullptr;
    socket_file_name_            = a_socket_file_name;
    socket_event_                = nullptr;
    socket_buffer_               = nullptr;
    socket_buffer_length_        = 0;
    one_shot_requests_handler_   = nullptr;
    keep_alive_requests_handler_ = nullptr;
    event_set_fatal_callback(ev::hub::Hub::EventFatalCallback);
#ifdef DEBUG
    event_set_log_callback(ev::hub::Hub::EventLogCallback);
#endif
    CC_IF_DEBUG_SET_VAR(thread_id_, cc::debug::Threading::k_invalid_thread_id_);
}

/**
 * @brief Destructor.
 */
ev::hub::Hub::~Hub ()
{
    Stop(-1);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Start the hub.
 *
 * @param a_initialized_callback
 * @param a_next_step_callback
 * @param a_publish_step_callback
 * @param a_disconnected_step_callback
 * @param a_device_step_factory
 * @param a_device_limits_step_callback
 */
void ev::hub::Hub::Start (ev::hub::Hub::InitializedCallback a_initialized_callback,
                          ev::hub::NextStepCallback a_next_step_callback,
                          ev::hub::PublishStepCallback a_publish_step_callback,
                          ev::hub::DisconnectedStepCallback a_disconnected_step_callback,
                          ev::hub::DeviceFactoryStepCallback a_device_step_factory,
                          ev::hub::DeviceLimitsStepCallback a_device_limits_step_callback)
{
    initialized_callback_ = a_initialized_callback;
    if ( nullptr == initialized_callback_ ) {
        throw ev::Exception("Unable to start hub loop: missing mandatory argument '%s'!",
                            "a_initialized_callback");
    }

    if ( nullptr == a_next_step_callback ) {
        throw ev::Exception("Unable to start hub loop: missing mandatory argument '%s'!",
                            "a_next_step_callback");
    }

    if ( nullptr == a_publish_step_callback ) {
        throw ev::Exception("Unable to start hub loop: missing mandatory argument '%s'!",
                            "a_publish_step_callback");
    }

    if ( nullptr == a_disconnected_step_callback ) {
        throw ev::Exception("Unable to start hub loop: missing mandatory argument '%s'!",
                            "a_disconnected_step_callback");
    }

    if ( nullptr == a_device_step_factory ) {
        throw ev::Exception("Unable to start hub loop: missing mandatory argument '%s'!",
                            "a_device_factory");
    }

    stepper_.next_         = new ev::hub::Hub::NextCallback(bridge_, a_next_step_callback);
    stepper_.publish_      = new ev::hub::Hub::PublishCallback(bridge_, a_publish_step_callback);
    stepper_.disconnected_ = new ev::hub::Hub::DisconnectedCallback(bridge_, a_disconnected_step_callback);
    stepper_.factory_      = std::move(a_device_step_factory);
    stepper_.limits_       = std::move(a_device_limits_step_callback);

    try {

        if ( nullptr != thread_ ) {
            throw ev::Exception("Unable to start hub loop: alread running!");
        }

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
        hack_event_ = evtimer_new(event_base_, ev::hub::Hub::LoopHackEventCallback, &hack_event_);
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
        watchdog_event_ = evtimer_new(event_base_, ev::hub::Hub::WatchdogCallback, this);
        if ( nullptr == watchdog_event_ ) {
            throw ev::Exception("Unable to start hub loop - can't create 'watchdog' event!");
        }
        const int wd_rv = evtimer_add(watchdog_event_, &tv);
        if ( wd_rv < 0 ) {
            throw ev::Exception("Unable to start hub loop: can't add 'watchdog' event - error code %d !",
                                wd_rv);
        }

        thread_ = new std::thread(&ev::hub::Hub::Loop, this);
        thread_->detach();

    } catch (const ev::Exception& a_ev_exception) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
		OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
}

/**
 * @brief Stop this hub.
 *
 * @param a_sig_no
 */
void ev::hub::Hub::Stop (int a_sig_no)
{

    OSALITE_DEBUG_TRACE("ev_hub", "~> @ %s(a_sig_no=%d)...", __FUNCTION__, a_sig_no);

    aborted_ = true;

    // TODO 2.0 - review -1 == a_sig_no - SHOULD ALWAYS WAIT FOR THREAD EXIT!
    if ( -1 == a_sig_no && true == running_ ) {
        //
        // SIGKILL || SIGTERM
        //
        // - THEADS WILL STOP RUNNING
        // - NO POINT IN WAITING FOR A THREAD THAT WONT RUN!
        //
        if ( nullptr != event_base_ ) {
            event_active(watchdog_event_, EV_TIMEOUT, 0);
        }
        OSALITE_DEBUG_TRACE("ev_hub", "-~ %s", "stop_cv_ .Wait() - in - from 'main' thread loop...");
        stop_cv_.Wait();
        OSALITE_DEBUG_TRACE("ev_hub", "-~ %s", "stop_cv_ .Wait() - out - from 'main' thread loop...");
    }

    running_ = false;

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
    }

    if ( nullptr != thread_ ) {
        delete thread_;
        thread_ = nullptr;
    }

    socket_.Close();

    if ( nullptr != socket_buffer_ ) {
        delete [] socket_buffer_;
        socket_buffer_ = nullptr;
        socket_buffer_length_ = 0;
    }

    handlers_.clear();

    if ( nullptr != stepper_.next_ ) {
        delete stepper_.next_;
        stepper_.next_ = nullptr;
    }

    if ( nullptr != stepper_.publish_ ) {
        delete stepper_.publish_;
        stepper_.publish_ = nullptr;
    }

    if ( nullptr != stepper_.disconnected_ ) {
        delete stepper_.disconnected_;
        stepper_.disconnected_ = nullptr;
    }

    stepper_.setup_   = nullptr;
    stepper_.factory_ = nullptr;

    OSALITE_DEBUG_TRACE("ev_hub", "<~ @ %s()...", __FUNCTION__);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Main loop for this hub.
 */
void ev::hub::Hub::Loop ()
{
    fault_msg_ = "";
    
    CC_IF_DEBUG_SET_VAR(thread_id_, cc::debug::Threading::GetInstance().CurrentThreadID());
    
    ::cc::threading::Worker::SetName(name_ + "::ev::hub");
    ::cc::threading::Worker::BlockSignals({SIGUSR1, SIGUSR2, SIGTTIN, SIGTERM, SIGQUIT});

    stepper_.setup_ = [this](ev::Device* a_device) {
        a_device->Setup(event_base_, [this] (const ev::Exception& a_ev_exception) {
            bridge_.ThrowFatalException(a_ev_exception);
        });
    };

    if ( nullptr != socket_event_ ) {
        event_free(socket_event_);
        socket_event_ = nullptr;
    }

    one_shot_requests_handler_   = new ev::hub::OneShotHandler(stepper_   CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));
    keep_alive_requests_handler_ = new ev::hub::KeepAliveHandler(stepper_ CC_IF_DEBUG_CONSTRUCT_APPEND_PARAM_VALUE(thread_id_));

    if ( false == socket_.Create(socket_file_name_.c_str()) ) {
        fault_msg_  = "Can't open a socket, using ";
        fault_msg_ += socket_file_name_;
        fault_msg_ += " file: ";
        fault_msg_ += socket_.GetLastConfigErrorString();
        fault_msg_ += "!";
        goto finally;
    }

	if ( false == socket_.Bind() ) {
        fault_msg_  = "Unable to bind hub socket ( " + socket_file_name_ + " ): ";
        fault_msg_ += socket_.GetLastConfigErrorString();
        fault_msg_ += "!";
        goto finally;
    }

    if ( false == socket_.SetNonBlock() ) {
        fault_msg_  = "Unable to set socket non-block property: ";
        fault_msg_ += socket_.GetLastConfigErrorString();
        fault_msg_ += "!";
        goto finally;
    }

    socket_event_ = event_new(event_base_, socket_.GetFileDescriptor(), EV_READ | EV_PERSIST, DatagramEventHandlerCallback, this);
    if ( nullptr == socket_event_ ) {
        fault_msg_ = "Unable to create an event for datagram socket!";
        goto finally;
    }

    timeval tv;

    tv.tv_sec  = 0;
    tv.tv_usec = 20000; // 20 milliseconds

    if ( 0 != event_add(socket_event_, &tv) ) {
        fault_msg_ = "Unable to add datagram socket event!";
        goto finally;
    }

    if ( 0 != fault_msg_.length() ) {
        goto finally;
    }

    try {
        initialized_callback_();
    } catch (const ev::Exception& a_ev_exception) {
        OSALITE_BACKTRACE();
        fault_msg_ = a_ev_exception.what();
        goto finally;
    } catch (const std::bad_alloc& a_bad_alloc) {
        OSALITE_BACKTRACE();
        fault_msg_ = "C++ Bad Alloc: ";
        fault_msg_ += a_bad_alloc.what();
        goto finally;
    } catch (const std::runtime_error& a_rte) {
        OSALITE_BACKTRACE();
        fault_msg_ = "C++ Runtime Error:";
        fault_msg_ += a_rte.what();
        goto finally;
    } catch (const std::exception& a_std_exception) {
        OSALITE_BACKTRACE();
        fault_msg_ = "C++ Standard Exception:";
        fault_msg_ += a_std_exception.what();
    }  catch (...) {
		OSALITE_BACKTRACE();
        fault_msg_ = STD_CPP_GENERIC_EXCEPTION_TRACE();
        goto finally;
    }

    running_  = true;
    handlers_ = { one_shot_requests_handler_, keep_alive_requests_handler_ };

#ifndef EVLOOP_NO_EXIT_ON_EMPTY
    #define EVLOOP_NO_EXIT_ON_EMPTY 0x04
#endif

    while ( false == aborted_ ) {

        int rv = event_base_loop(event_base_, EVLOOP_NO_EXIT_ON_EMPTY);
        OSALITE_DEBUG_TRACE("ev_hub", "-~ event_base_loop rv is %d", rv);
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

finally:

    OSALITE_DEBUG_TRACE("ev_hub", "-~ closing socket %d", socket_.GetFileDescriptor());
    
    if ( nullptr != one_shot_requests_handler_ ) {
        delete one_shot_requests_handler_;
        one_shot_requests_handler_ = nullptr;
    }

    if ( nullptr != keep_alive_requests_handler_ ) {
        delete keep_alive_requests_handler_;
        keep_alive_requests_handler_ = nullptr;
    }
    
    socket_.Close();
   
    handlers_.clear();

    if ( 0 != fault_msg_.length() ) {
        bridge_.ThrowFatalException(ev::Exception(fault_msg_));
    }

    OSALITE_DEBUG_TRACE("ev_hub", "-~ %s", "stop_cv_ .Wake() - in - from 'main' thread loop...");
    stop_cv_.Wake();
    OSALITE_DEBUG_TRACE("ev_hub", "-~ %s", "stop_cv_ .Wake() - out - from 'main' thread loop...");

    running_ = false;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Perform a sanity check.
 */
void ev::hub::Hub::SanityCheck ()
{
    try {

        CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

        if ( nullptr != one_shot_requests_handler_ ) {
            one_shot_requests_handler_->SanityCheck();
        }

        if ( nullptr != keep_alive_requests_handler_ ) {
            keep_alive_requests_handler_->SanityCheck();
        }

    } catch (const ev::Exception& a_ev_exception) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
		OSALITE_BACKTRACE();
        bridge_.ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
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
void ev::hub::Hub::EventFatalCallback (int a_error)
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
void ev::hub::Hub::EventLogCallback (int a_severity, const char* a_msg)
{
    const char* s;
    switch (a_severity) {
        case _EVENT_LOG_DEBUG: s = "debug"; break;
        case _EVENT_LOG_MSG:   s = "msg";   break;
        case _EVENT_LOG_WARN:  s = "warn";  break;
        case _EVENT_LOG_ERR:   s = "error"; break;
        default:               s = "?";     break; /* never reached */
    }
    fprintf(stderr, "Log: [%5s] %s\n", s, a_msg);
    fflush(stderr);
}

/**
 * @brief This is a hack to prevent event_base_loop from exiting;
 *         The flag EVLOOP_NO_EXIT_ON_EMPTY is somehow ignored, at least on Mac OS X.
 *
 * @param a_fd    A file descriptor or signal.
 * @param a_flags One or mode EV_* flags.
 * @param a_arg   User-supplied argument.
 */
void ev::hub::Hub::LoopHackEventCallback (evutil_socket_t /* a_fd */, short /* a_flags */, void* a_arg)
{
    timeval tv;
    tv.tv_sec  = 365 * 24 * 3600;
    tv.tv_usec = 0;
    event *ev = *(event **)a_arg;
    (void)evtimer_add(ev, &tv);
}

/**
 * @brief Handle datagram messages send by 'main' thread to ev::Hub thread.
 *
 * @param a_df
 * @param a_flags
 * @param a_arg
 */
void ev::hub::Hub::DatagramEventHandlerCallback (evutil_socket_t a_fd, short /* a_flags */, void* a_arg)
{
    // OSALITE_DEBUG_TRACE("ev_hub", "~> %s(...)", __FUNCTION__);
                        
    ev::hub::Hub* self = (ev::hub::Hub*)a_arg;

    if ( self->socket_.GetFileDescriptor() != a_fd /* || a_flags != EV_READ */ ) {
        return;
    }

    if ( nullptr == self->socket_buffer_ ) {
        self->socket_buffer_        = new uint8_t[4096];
        self->socket_buffer_length_ = 4096;
    }

    int msg_received  = 0;
    int rx_count      = 0;
    int mgs_remaining = 0;

    // ... while bytes available ...
    while ( true ) {

        size_t length = 0;
        if ( false == self->socket_.Receive(self->socket_buffer_, self->socket_buffer_length_, length) ) {

            const int last_error = self->socket_.GetLastReceiveError();
            if ( EAGAIN == last_error ) {
                break;
            } else if ( 0 == last_error ) { // no messages are available to be received and the peer has performed an orderly shutdown
                // TODO CONNECTORS throw ev::Exception("No messages are available to be received and the peer has performed an orderly shutdown!");
                break;
            } else {
                throw ev::Exception("Unable to read data from socket : %d - %s!",
                                    last_error,
                                    self->socket_.GetLastReceiveErrorString().c_str()
                );
            }

        }

        mgs_remaining = ( std::atomic_fetch_add(&self->pending_callbacks_count_, -1) - 1 );
        msg_received += 1;
        rx_count     += length;

        OSALITE_DEBUG_TRACE("ev_hub",
                            "-~ eh: received %d message(s) [ %d byte(s) ], pending %d message(s)",
                            msg_received, rx_count, mgs_remaining
        );
        (void)mgs_remaining;


        // <invoke_id>:<mode>:<target>:<tag>:<obj_addr>

        // ... ensure message has the minimum length ...
        if ( length < k_msg_min_length_ ) {
            OSALITE_DEBUG_TRACE("ev_hub", "-~ Skipping message... " SIZET_FMT " vs " SIZET_FMT,
                                length, k_msg_min_length_);
            continue;
        }

        const std::string message = std::string(reinterpret_cast<char*>(self->socket_buffer_), length);

        // ... split message required components ...

        const char* msg_ctr_ = message.c_str();

        // ... read: invoke id ...
        const char* invoke_id_ptr = msg_ctr_;
        if ( nullptr == invoke_id_ptr ) {
            self->bridge_.ThrowFatalException(ev::Exception("Invalid '%s' value: nullptr!", "invoke id"));
            return;
        }

        int     tmp_number;
        int64_t invoke_id;
        if ( 1 != sscanf(invoke_id_ptr, "%d:", &tmp_number) ) {
            self->bridge_.ThrowFatalException(ev::Exception("Unable to read '%s' value!", "invoke id"));
            return;
        }
        invoke_id = static_cast<int64_t>(tmp_number);

        // ... read: mode, one of \link ev::Request::Mode \link  ...
        const char* mode_ptr = strchr(invoke_id_ptr, ':');
        if ( nullptr == mode_ptr ) {
            self->bridge_.ThrowFatalException(ev::Exception("Invalid '%s' value: nullptr!", "mode"));
            return;
        }
        mode_ptr = mode_ptr + sizeof(char);

        uint8_t mode;
        if ( 1 != sscanf(mode_ptr, "%d:", &tmp_number) ) {
            self->bridge_.ThrowFatalException(ev::Exception("Unable to read '%s' value!", "mode"));
            return;
        }
        mode = static_cast<uint8_t>(tmp_number);

        // ... read: target, one of ev::Object::Target ...
        const char* target_ptr = strchr(mode_ptr, ':');
        if ( nullptr == target_ptr ) {
            self->bridge_.ThrowFatalException(ev::Exception("Invalid '%s' value: nullptr!", "target"));
            return;
        }
        target_ptr = target_ptr + sizeof(char);

        uint8_t target;
        if ( 1 != sscanf(target_ptr, "%d:", &tmp_number) ) {
            self->bridge_.ThrowFatalException(ev::Exception("Unable to read '%s' value!", "target"));
            return;
        }
        target = static_cast<uint8_t>(tmp_number);
        // ... read: tag, uint8_t ...
        const char* tag_ptr = strchr(target_ptr, ':');
        if ( nullptr == tag_ptr ) {
            self->bridge_.ThrowFatalException(ev::Exception("Invalid '%s' value: nullptr!", "tag"));
            return;
        }
        tag_ptr = tag_ptr + sizeof(char);

        uint8_t tag;
        if ( 1 != sscanf(tag_ptr, "%d:", &tmp_number) ) {
            self->bridge_.ThrowFatalException(ev::Exception("Unable to read '%s' value!", "tag"));
            return;
        }
        tag = static_cast<uint8_t>(tmp_number);

        //
        // ... read object addr ...
        //
        ev::Request* request;
        const char* payload_ptr = strchr(tag_ptr, ':');
        if ( nullptr != payload_ptr ) {
            payload_ptr = payload_ptr + sizeof(char);
            void* object;
            if ( 1 != sscanf(payload_ptr, "%p", &object) ) {
                self->bridge_.ThrowFatalException(ev::Exception("Unable to read object address!"));
                return;
            }
            request = static_cast<ev::Request*>(object);
        } else {
            request = nullptr;
        }

        switch (static_cast<ev::Object::Target>(target)) {

            case ev::Object::Target::Redis:
            case ev::Object::Target::PostgreSQL:
            case ev::Object::Target::CURL:
            {
                if ( nullptr == request ) {
                    self->bridge_.ThrowFatalException(ev::Exception("Expecting a valid request, got nullptr!"));
                    return;
                }
                request->Set(/* a_invoke_id */invoke_id, /* a_tag */ tag);
                switch (static_cast<ev::Request::Mode>(mode)) {
                    case ev::Request::Mode::OneShot:
                        try {
                            self->one_shot_requests_handler_->Push(request);
                        } catch (const ev::Exception& a_ev_exception) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(a_ev_exception);
                        } catch (const std::bad_alloc& a_bad_alloc) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
                        } catch (const std::runtime_error& a_rte) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
                        } catch (const std::exception& a_std_exception) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
                        } catch (...) {
							OSALITE_BACKTRACE();
							self->bridge_.ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
                        }
                        break;
                    case ev::Request::Mode::KeepAlive:
                        try {
                            self->keep_alive_requests_handler_->Push(request);
                        } catch (const ev::Exception& a_ev_exception) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(a_ev_exception);
                        } catch (const std::bad_alloc& a_bad_alloc) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
                        } catch (const std::runtime_error& a_rte) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
                        } catch (const std::exception& a_std_exception) {
                            OSALITE_BACKTRACE();
                            self->bridge_.ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
                        } catch (...) {
							OSALITE_BACKTRACE();
							self->bridge_.ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
                        }
                        break;
                    default:
                        OSALITE_BACKTRACE();
                        self->bridge_.ThrowFatalException(ev::Exception("Unknown target " UINT8_FMT " !", target));
                        return;
                }
                break;
            }

            case ev::Object::Target::NotSet:
            {
                try {

                    typedef struct _NextStepPayload {
                        int64_t invoke_id_;
                        uint8_t mode_;
                        uint8_t target_;
                        uint8_t tag_;

                    } NextStepPayload;

                    NextStepPayload* p = new NextStepPayload ({invoke_id, mode, target, tag});

                    self->stepper_.next_->Call(
                                               [CC_IF_DEBUG_LAMBDA_CAPTURE(self,) p] () -> void* {
                                                   CC_DEBUG_FAIL_IF_NOT_AT_THREAD(self->thread_id_);
                                                   return p;
                                               },
                                               [](void* a_payload, ev::hub::NextStepCallback a_callback) {
                                                   CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                                                   NextStepPayload* pp = static_cast<NextStepPayload*>(a_payload);
                                                   // ... return is ignore, because we did not transfer the 'result' object ( ownership ) ...
                                                   (void)a_callback(pp->invoke_id_, static_cast<ev::Object::Target>(pp->target_), pp->tag_, nullptr);

                                                   delete pp;
                                               }
                    );

                } catch (const ev::Exception& a_ev_exception) {
                    OSALITE_BACKTRACE();
                    self->bridge_.ThrowFatalException(a_ev_exception);
                } catch (const std::bad_alloc& a_bad_alloc) {
                    OSALITE_BACKTRACE();
                    self->bridge_.ThrowFatalException(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
                } catch (const std::runtime_error& a_rte) {
                    OSALITE_BACKTRACE();
                    self->bridge_.ThrowFatalException(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
                } catch (const std::exception& a_std_exception) {
                    OSALITE_BACKTRACE();
                    self->bridge_.ThrowFatalException(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
                } catch (...) {
					OSALITE_BACKTRACE();
					self->bridge_.ThrowFatalException(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
                }
                break;
            }

            default:
            {
                OSALITE_BACKTRACE();
                self->bridge_.ThrowFatalException(ev::Exception("Unknown target " UINT8_FMT " !", target));
                return;
            }
        }

    }

    // OSALITE_DEBUG_TRACE("ev_hub", "-~ %s", "Idle...");
    self->one_shot_requests_handler_->Idle();
    self->keep_alive_requests_handler_->Idle();
    
    // OSALITE_DEBUG_TRACE("ev_hub", "<~ %s(...)", __FUNCTION__);
}

/**
 * @brief Handle event to break base.
 *
 * @param a_df
 * @param a_flags
 * @param a_arg
 */
void ev::hub::Hub::WatchdogCallback (evutil_socket_t /* a_fd */, short /* a_flags */, void* a_arg)
{
    ev::hub::Hub* self = (ev::hub::Hub*)a_arg;

    OSALITE_DEBUG_TRACE("ev_hub", "~> @ %s -> Watchdog reporting for duty...", __FUNCTION__);

    if ( true == self->aborted_ ) {
        OSALITE_DEBUG_TRACE("ev_hub", "~> @ %s -> Watchdog decision is... break it!", __FUNCTION__);
        event_base_loopbreak(self->event_base_);
    } else {
        OSALITE_DEBUG_TRACE("ev_hub", "~> @ %s -> Watchdog decision is... let it lingering!", __FUNCTION__);
        timeval tv;
        tv.tv_sec  = 365 * 24 * 3600;
        tv.tv_usec = 0;
        event *ev = *(event **)self->watchdog_event_;
        (void)evtimer_add(ev, &tv);
    }
}
