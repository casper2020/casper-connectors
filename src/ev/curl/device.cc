/**
 * @file device.cc
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

#include "ev/curl/device.h"

#include "ev/curl/reply.h"
#include "ev/curl/error.h"

#include "osal/osalite.h"

#include <algorithm> // std::min

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 */
ev::curl::Device::Device (const ::ev::Loggable::Data& a_loggable_data)
    : ev::Device(a_loggable_data)
{
    context_ = nullptr;
}

/**
 * @brief Destructor.
 */
ev::curl::Device::~Device ()
{
    Disconnect(/* a_notify */ false);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Establish an HTTP connection.
 *
 * @return One of \link ev::curl::Device::Status \link.
 */
ev::curl::Device::Status ev::curl::Device::Connect (ev::curl::Device::ConnectedCallback a_callback)
{
    // ... device is not properly setup ...
    if ( nullptr == event_base_ptr_ ) {
        return ev::curl::Device::Status::Error;
    }

    // ... new 'connection'?
    if ( nullptr == context_ ) {
        context_ = new ev::curl::Device::MultiContext(this);
        if ( true == context_->ContainsErrors() ) {
            // ... forget it ...
            delete context_;
            context_ = nullptr;
            // ... report error ...
            return ev::curl::Device::Status::Error;
        }
    }
    
    // .. mark as disconnected ...
    connection_status_ = ev::Device::ConnectionStatus::Connected;

    // ... keep track of 'connection' callback ...
    a_callback(ev::curl::Device::ConnectionStatus::Connected, this);
    connected_callback_ = nullptr;

    // ... done ...
    return ev::curl::Device::Status::Nop;
}

/**
 * @brief Close current HTTP connection.
 *
 * @param a_callback
 *
 * @return One of \link ev::curl::Device::Status \link.
 */
ev::curl::Device::Status ev::curl::Device::Disconnect (ev::curl::Device::DisconnectedCallback a_callback)
{
    // ... not connected?
    if ( nullptr == context_ ) {
        return ev::curl::Device::Status::Nop;
    }
    // ... keep track of callback ...
    disconnected_callback_ = a_callback;
    // ... common disconnect ...
    Disconnect();
    // ... done ...
    return ev::curl::Device::Status::Nop;
}

/**
 * @brief Setup to send or received data from an HTTP connection
 *
 * @param a_callback
 * @param a_format
 * @param ...
 *
 * @return One of \link ev::curl::Device::Status \link.
 */
ev::curl::Device::Status ev::curl::Device::Execute (ev::curl::Device::ExecuteCallback a_callback, const ev::Request* a_request)
{
    ev::Request* rw_request = const_cast<ev::Request*>(a_request);
    
    // ... reset error ...
    last_error_msg_ = "";

    ev::curl::Request* curl_request = dynamic_cast<ev::curl::Request*>(rw_request);
    if ( nullptr == curl_request ) {
        // ... set error message ...
        last_error_msg_ = "Can't execute a nullptr request!";
        // ... we're done ..
        return ev::curl::Device::Status::Error;
    }

    // ... no connection?
    if ( nullptr == context_ ) {
        // ... set error message ...
        last_error_msg_ = "Context not set!";
        // ... we're done ..
        return ev::curl::Device::Status::Error;
    }

    CURL* easy_handle = curl_request->Setup();
    if ( nullptr == easy_handle ) {
        // ... set error message ...
        last_error_msg_   = "Easy handle not set!";
        // ... we're done ..
        return ev::curl::Device::Status::Error;
    }

    // ... map easy_handle to request & callback .../
    if ( map_.end() != map_.find(easy_handle) ) {
        // ... set error message ...
        last_error_msg_   = "Trying to insert a duplicated easy handle into the multi context!";
        // ... we're done ..
        return ev::curl::Device::Status::Error;
    }
    map_[easy_handle] = { curl_request, a_callback };
    
    // ... track start time point ...
    curl_request->SetStarted();

    // ... add easy handle to multi handle ...
    const CURLMcode rc = curl_multi_add_handle(context_->handle_, easy_handle);
    if ( CURLM_OK != rc ) {
        // ... untrack callback ...
        map_.erase(map_.find(easy_handle));
        // ... set error message ...
        last_error_msg_   = "Unable to add easy handle to multi context!";
        // ... track end time point ...
        curl_request->SetFinished();
        // ... we're done ..
        return ev::curl::Device::Status::Error;
    }
    // ... we're done ...
    return ev::curl::Device::Status::Async;
}

/**
 * @return The last set error object, nullptr if none.
 */
ev::Error* ev::curl::Device::DetachLastError ()
{
    if ( 0 != last_error_msg_.length() ) {
        if ( CURLE_OK != last_error_code_ ) {
            return new ev::curl::Error((CURLcode)last_error_code_, last_error_msg_);
        } else {
            return new ev::curl::Error(last_error_msg_);
        }
    } else {
        return nullptr;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Release current curl connection.
 *
 * @param a_notify When true listener will be notified.
 */
void ev::curl::Device::Disconnect (bool a_notify)
{
    // ... already disconnected?
    if ( nullptr == context_ ) {
        // ... notify?
        if ( nullptr != disconnected_callback_ ) {
            disconnected_callback_(connection_status_, this);
            disconnected_callback_ = nullptr;
        }
        // ... nothing else to do ...
        return;
    }
    // ... remove event and notify ...
    try {
        // ... release event ...
        if ( nullptr != context_->event_ ) {
            const int del_rc = event_del(context_->event_);
            if ( 0 != del_rc ) {
                exception_callback_(ev::Exception("An error occurred while deleting an event: code %d!", del_rc));
            }
            event_del(context_->event_);
            event_free(context_->event_);
            context_->event_ = nullptr;
        }
        // ... release connection ...
        if ( nullptr != context_->handle_ ) {
            curl_multi_cleanup(context_->handle_);
            context_->handle_ = nullptr;
        }
        // .. mark as disconnected ...
        connection_status_ = ev::Device::ConnectionStatus::Disconnected;
        // ... if we're listening for a connection ...
        if ( nullptr != connected_callback_ ) {
            connected_callback_(connection_status_, this);
            connected_callback_ = nullptr;
        }
        // ... if we're waiting for a execution ...
        if ( nullptr != execute_callback_ ) {
            ev::Result* result = new ev::Result(ev::Object::Target::CURL);
            ev::Error*  error  = DetachLastError();
            if ( nullptr != error ) {
                result->AttachDataObject(error);
            } else {
                result->AttachDataObject(new ev::curl::Error("Disconnected from CURL server!"));
            }
            execute_callback_(ev::Device::ExecutionStatus::Error, result);
            execute_callback_ = nullptr;
        }
        // ... if we're waiting for a disconnect request ...
        if ( nullptr != disconnected_callback_ ) {
            disconnected_callback_(connection_status_, this);
            disconnected_callback_ = nullptr;
        }
        // ... notify all listeners ...
        if ( nullptr != listener_ptr_ && true ==a_notify ) {
            listener_ptr_->OnConnectionStatusChanged(connection_status_, this);
        }
    } catch (const ev::Exception& a_ev_exception) {
        OSALITE_BACKTRACE();
        exception_callback_(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }

}

#ifdef __APPLE__
#pragma mark - MultiContext
#endif

/**
 * @brief Default constructor.
 */
ev::curl::Device::MultiContext::MultiContext (ev::curl::Device* a_device)
{
    device_ptr_ = a_device;
    handle_     = curl_multi_init();
    if ( nullptr == handle_ ) {
        last_code_    = CURLM_BAD_HANDLE;
        setup_errors_ = -1;
    } else {
        setup_errors_  = curl_multi_setopt(handle_, CURLMOPT_SOCKETFUNCTION, ev::curl::Device::MultiContext::SocketCallback);
        setup_errors_ += curl_multi_setopt(handle_, CURLMOPT_SOCKETDATA    , this);
        setup_errors_ += curl_multi_setopt(handle_, CURLMOPT_TIMERFUNCTION , ev::curl::Device::MultiContext::TimerCallback);
        setup_errors_ += curl_multi_setopt(handle_, CURLMOPT_TIMERDATA     , this);
        setup_errors_ += curl_multi_setopt(handle_, CURLMOPT_PIPELINING, 0);
        setup_errors_ += curl_multi_setopt(handle_, CURLMOPT_MAXCONNECTS, 1L);
        setup_errors_ += curl_multi_setopt(handle_, CURLMOPT_MAX_TOTAL_CONNECTIONS, 1L);

        if ( 0 != setup_errors_ ) {
            curl_multi_cleanup(handle_);
            handle_    = nullptr;
            last_code_ = CURLM_BAD_HANDLE;
        } else {
            last_code_ = CURLM_OK;
        }
    }
    last_exec_code_        = CURLE_FAILED_INIT;
    last_http_status_code_ = 500;
    still_running_         = 0;
    event_                 = nullptr;
    timer_event_           =  evtimer_new(device_ptr_->event_base_ptr_, ev::curl::Device::MultiContext::EventTimerCallback, this);
}

/**
 * @brief Destructor.
 */
ev::curl::Device::MultiContext::~MultiContext ()
{
    if ( nullptr != handle_ ) {
        curl_multi_cleanup(handle_);
    }
    if ( nullptr != event_ ) {
        event_del(event_);
        event_free(event_);
    }
    if ( nullptr != timer_event_ ) {
        event_del(timer_event_);
        event_free(timer_event_);
    }
}

/**
 * @brief Check if an error is set.
 *
 * @return True an error was set, false otherwise.
 */
bool ev::curl::Device::MultiContext::ContainsErrors () const
{
    return ( 0 != setup_errors_ ) || ( CURLM_OK != last_code_ );
}

/**
 * @brief Check for bad connections and multi handle for completed requests.
 *
 * @param a_context
 * @param a_code
 * @param a_where
 */
void ev::curl::Device::MultiContext::Process (ev::curl::Device::MultiContext* a_context,
                                              CURLMcode a_code, const char* /* a_where */)
{
    //  ... first, check for bad connections ...
    if ( CURLM_OK != a_code ) {
        const char *code_c_str;
        switch(a_code) {
            case
            CURLM_BAD_SOCKET:
                code_c_str = "CURLM_BAD_SOCKET";
                break;
            case CURLM_BAD_HANDLE:
                code_c_str = "CURLM_BAD_HANDLE";
                break;
            case CURLM_BAD_EASY_HANDLE:
                code_c_str = "CURLM_BAD_EASY_HANDLE";
                break;
            case CURLM_OUT_OF_MEMORY:
                code_c_str = "CURLM_OUT_OF_MEMORY";
                break;
            case CURLM_INTERNAL_ERROR:
                code_c_str = "CURLM_INTERNAL_ERROR";
                break;
            case CURLM_UNKNOWN_OPTION:
                code_c_str = "CURLM_UNKNOWN_OPTION";
                break;
            case CURLM_LAST:
                code_c_str = "CURLM_LAST";
                break;
            default:
                code_c_str = "CURLM_unknown";
                break;
        }
        // ... this is a critical error !
        a_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while validating a multi request: code %d %s !", a_code, code_c_str));
    }

    // ... check multi handle for completed requests ...
    CURLMsg*  current;
    int       remaining;

    while ( nullptr != ( current = curl_multi_info_read(handle_, &remaining) ) ) {
        
        // not ready?
        if ( CURLMSG_DONE != current->msg ) {
            // ... try next ...
            continue;
        }
        
        last_exec_code_        = current->data.result;
        last_http_status_code_ = 500;

        CURL* easy = current->easy_handle;
        
        if ( CURLE_OK != curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &last_http_status_code_) ) {
            device_ptr_->last_error_msg_ = "Unable to obtain a valid HTTP status code!";
        }
        
        // ... remove easy handle ...
        curl_multi_remove_handle(handle_, easy);

        const auto it = device_ptr_->map_.find(easy);
        if ( device_ptr_->map_.end() == it ) {
            // ... insanity checkpoint reached!
            continue;
        }

        // ... from now one, don't call curl_easy_cleanup(easy) - it's owned by ev::curl::Request instance !
        ev::Result* result = new ev::Result(ev::Object::Target::CURL);
        
        device_ptr_->last_error_code_ = (int64_t)last_exec_code_;
        if ( CURLE_OK != last_exec_code_ ) {
            const char* error_c_str = curl_easy_strerror(last_exec_code_);
            if ( nullptr != error_c_str && '\0' != error_c_str[0] ) {
                device_ptr_->last_error_msg_ = error_c_str;
            } else {
                switch(last_exec_code_) {
                    case CURLE_OK:
                        break;
                    case CURLE_URL_MALFORMAT:
                        device_ptr_->last_error_msg_ = "CURLE_URL_MALFORMAT";
                        break;
                    case CURLE_COULDNT_RESOLVE_HOST:
                        device_ptr_->last_error_msg_ = "CURLE_COULDNT_RESOLVE_HOST";
                        break;
                    case CURLE_COULDNT_RESOLVE_PROXY:
                        device_ptr_->last_error_msg_ = "CURLE_COULDNT_RESOLVE_PROXY";
                        break;
                    case CURLE_COULDNT_CONNECT:
                        device_ptr_->last_error_msg_ = "CURLE_COULDNT_CONNECT";
                        break;
                    case CURLE_OPERATION_TIMEDOUT:
                        device_ptr_->last_error_msg_ = "CURLE_OPERATION_TIMEDOUT";
                        break;
                    case CURLE_HTTP_POST_ERROR:
                        device_ptr_->last_error_msg_ = "CURLE_HTTP_POST_ERROR";
                        break;
                    case CURLE_ABORTED_BY_CALLBACK:
                        device_ptr_->last_error_msg_ = "CURLE_ABORTED_BY_CALLBACK";
                        break;
                    default:
                        device_ptr_->last_error_msg_ = "CURLE : " + std::to_string(last_exec_code_);
                        break;
                }
            }
        }

        // ... flush ...
        it->second.request_ptr_->Close();
        
        // ... track end time point ...
        it->second.request_ptr_->SetFinished();

        // ... attach result or error ...
        if ( 0 == device_ptr_->last_error_msg_.length() ) {            
            auto reply = new ev::curl::Reply(static_cast<int>(last_http_status_code_),
                                             it->second.request_ptr_->rx_headers(),
                                             it->second.request_ptr_->AsString(),
                                             it->second.request_ptr_->Elapsed()
            );
            CC_IF_DEBUG_ELSE({
                const auto& debug = it->second.request_ptr_->debug();
                reply->SetInfo(easy, ( true == debug.enabled_ ? &debug.data_ : nullptr ));
            }, {
                reply->SetInfo(easy);
            });
            // ... result ...
            result->AttachDataObject(reply);
        } else {
            // ... error ...
            result->AttachDataObject(device_ptr_->DetachLastError());
            // ... rx exception ...
            if ( nullptr != it->second.request_ptr_->rx_exp() ) {
                result->AttachDataObject(new ev::curl::Error("%s", it->second.request_ptr_->rx_exp()->what()));
            }
            // ... tx exception ...
            if ( nullptr != it->second.request_ptr_->tx_exp() ) {
                result->AttachDataObject(new ev::curl::Error("%s", it->second.request_ptr_->tx_exp()->what()));
            }
        }
        // ... notify ...
        it->second.exec_callback_(
            (( CURLE_OK == last_exec_code_ ) > 0 ? ev::Device::ExecutionStatus::Error : ev::Device::ExecutionStatus::Ok ),
            result
        );
        // ... forget it !
        device_ptr_->map_.erase(it);
    }
}

/**
 * @brief Called by libcurl, to notify about a socket update.
 *
 * @param a_handle
 * @param a_socket
 * @param a_what
 * @param a_user_ptr
 * @param a_socket_ptr
 *
 * @remarks: CURLMOPT_SOCKETFUNCTION
 */
int ev::curl::Device::MultiContext::SocketCallback (CURL* a_handle, curl_socket_t a_socket, int a_what, void* a_user_ptr, void* a_socket_ptr)
{
    ev::curl::Device::MultiContext*                multi_context  = static_cast<ev::curl::Device::MultiContext*>(a_user_ptr);
    ev::curl::Device::MultiContext::SocketContext* socket_context = static_cast<ev::curl::Device::MultiContext::SocketContext*>(a_socket_ptr);

    // The what parameter informs the callback on the status of the given socket. It can hold one of these values:
    switch(a_what)
    {
        case CURL_POLL_REMOVE:
        {
            // ... forget socket context ...
            if ( nullptr != socket_context ) {
                delete socket_context;
            }
            // ... clear assignment ...
            const CURLMcode cm_assign_rc = curl_multi_assign(multi_context->handle_, a_socket, nullptr);
            if ( CURLM_OK != cm_assign_rc ) {
                multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while removing a socket multi handle assignment: code %d!", cm_assign_rc));
            }
            // ... done ...
            break;
        }

        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT:
        {
            // ... new socket?
            if ( nullptr == socket_context ) {
                socket_context = new ev::curl::Device::MultiContext::SocketContext();
            }

            // ... set socket ...
            const short action  = ( a_what & CURL_POLL_IN ? EV_READ : 0 ) | ( a_what & CURL_POLL_OUT ? EV_WRITE : 0 ) | EV_PERSIST;
            const bool new_event = ( nullptr == socket_context->event_ );

            socket_context->fd_              = a_socket;
            socket_context->event_action_    = action;
            socket_context->easy_handle_ptr_ = a_handle;

            if ( true == new_event ) {
                socket_context->event_ = event_new(multi_context->device_ptr_->event_base_ptr_, socket_context->fd_, action, ev::curl::Device::MultiContext::EventCallback, multi_context);
            } else {
                const int ev_del_rc = event_del(socket_context->event_);
                if ( 0 != ev_del_rc ) {
                    multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while deleting a socket event: code %d!", ev_del_rc));
                }
            }
            
            const int ev_assign_rc = event_assign(socket_context->event_, multi_context->device_ptr_->event_base_ptr_, socket_context->fd_, action, ev::curl::Device::MultiContext::EventCallback, multi_context);
            if ( 0 != ev_assign_rc ) {
                multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while removing a socket multi handle assigment: code %d!", ev_assign_rc));
            }

            const int ev_add_rc = event_add(socket_context->event_, nullptr);
            if ( 0 != ev_add_rc ) {
                multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while adding a socket event: code %d!", ev_add_rc));
            }

            if ( true == new_event ) {
                const CURLMcode cm_assign_rc = curl_multi_assign(multi_context->handle_, a_socket, socket_context);
                if ( CURLM_OK != cm_assign_rc ) {
                    multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while adding a socket multi handle assigment: code %d!", cm_assign_rc));
                }
            }

            // ... done ...
            break;
        }

        default:
            // ... nothing to do ...
            break;
    }

    return CURLM_OK;
}

/*
 * @brief Called by libcurl, multi context, to update the event timer after curl_multi library calls.
 *
 * @param a_handle
 * @param a_timeout_ms
 * @param a_user_ptr
 *
 * @remarks: CURLMOPT_TIMERFUNCTION
 */
int ev::curl::Device::MultiContext::TimerCallback (CURLM* a_handle, long a_timeout_ms, void* a_user_ptr)
{
    ev::curl::Device::MultiContext* multi_context = static_cast<ev::curl::Device::MultiContext*>(a_user_ptr);
    
    if ( a_handle != multi_context->handle_ ) {
        multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while deleting a timer event: invalid handle!"));
    } else {
        struct timeval timeout;
        timeout.tv_sec  = a_timeout_ms / 1000;
        timeout.tv_usec = ( a_timeout_ms % 1000 ) * 1000;
        if ( -1 == a_timeout_ms ) {
            if ( 1 == evtimer_pending(multi_context->timer_event_, NULL) ) {
                const int ev_del_rc = evtimer_del(multi_context->timer_event_);
                if ( 0 != ev_del_rc ) {
                    multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while deleting a timer event: code %d!", ev_del_rc));
                }
            }
        } else {
            if ( 0 == evtimer_pending(multi_context->timer_event_, NULL) ) {
                const int ev_add_rc = evtimer_add(multi_context->timer_event_, &timeout);
                if ( 0 != ev_add_rc ) {
                    multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while adding a timer event: code %d!", ev_add_rc));
                }
            }
        }
    }
    
    return CURLM_OK;
}

/**
 * @brief Called by libevent when we get action on a multi socket.
 *
 * @param a_fd
 * @param a_kind
 * @param a_context
 */
void ev::curl::Device::MultiContext::EventCallback (int a_fd, short a_kind, void* a_context)
{
    ev::curl::Device::MultiContext* multi_context = static_cast<ev::curl::Device::MultiContext*>(a_context);

    const int       action = ( a_kind & EV_READ ? CURL_CSELECT_IN : 0 ) | ( a_kind & EV_WRITE ? CURL_CSELECT_OUT : 0 );
    const CURLMcode rc     = curl_multi_socket_action(multi_context->handle_, a_fd, action, &multi_context->still_running_);

    multi_context->Process(multi_context, rc, __FUNCTION__);

    if ( multi_context->still_running_ <= 0 ) {
        // ... last transfer done, kill timeout ...
        if ( evtimer_pending(multi_context->timer_event_, NULL) ) {
            const int ev_del_rc = evtimer_del(multi_context->timer_event_);
            if ( 0 != ev_del_rc ) {
                multi_context->device_ptr_->exception_callback_(ev::Exception("An error occurred while deleting a timer event: code %d!", ev_del_rc));
            }
        }
    }
}

/**
 * @brief Called by libevent when we get action on a multi socket.
 *
 * @param a_fd
 * @param a_kind
 * @param a_context
 */
void ev::curl::Device::MultiContext::EventTimerCallback (int /* a_fd */, short /* a_kind */, void* a_context)
{
    ev::curl::Device::MultiContext* context = static_cast<ev::curl::Device::MultiContext*>(a_context);

    const CURLMcode rc = curl_multi_socket_action(context->handle_, CURL_SOCKET_TIMEOUT, 0, &context->still_running_);

    context->Process(context, rc, __FUNCTION__);
}

#ifdef __APPLE__
#pragma mark - MultiContext::SocketContext
#endif

/**
 * @brief Default constructor.
 */
ev::curl::Device::MultiContext::SocketContext::SocketContext ()
{
    fd_              = CURL_SOCKET_BAD;
    timeout_         = 0;
    event_action_    = 0;
    event_           = nullptr;
    easy_handle_ptr_ = nullptr;
    context_ptr_     = nullptr;
    request_ptr_     = nullptr;
}

/**
 * @brief Default constructor.
 */
ev::curl::Device::MultiContext::SocketContext::~SocketContext ()
{
    if ( nullptr != event_ ) {
        event_del(event_);
        event_free(event_);
    }
}
