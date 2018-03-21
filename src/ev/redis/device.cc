/**
 * @file device.c - REDIS
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

#include "ev/redis/device.h"

#include "ev/redis/error.h"

#include "ev/redis/subscriptions/reply.h"

#include "ev/logger.h"

#include "osal/osalite.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_ip_address
 * @param a_port_number
 * @param a_database_index
 */
ev::redis::Device::Device (const Loggable::Data& a_loggable_data,
                           const char* const a_ip_address, const int a_port_number, const int a_database_index)
    : ev::Device(a_loggable_data),
      ip_address_(a_ip_address), port_number_(a_port_number), database_index_(a_database_index)
{
    request_ptr_       = nullptr;
    hiredis_context_   = nullptr;
    database_request_  = nullptr;
    database_selected_ = false;
}

/**
 * @brief Destructor.
 */
ev::redis::Device::~Device ()
{
    if ( nullptr != hiredis_context_ ) {
        hiredis_context_->data = nullptr;
        // can't call redisFree: wrong context ...
        // ... and can't call redisAsyncFree: can't reset callbacks ...
        // ... it should be released when connection fails ...
        hiredis_context_ = nullptr;
    }
    if ( nullptr != database_request_ ) {
        delete database_request_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Connect to a REDIS database.
 *
 * @return One of \link ev::redis::Device::Status \link.
 */
ev::redis::Device::Status ev::redis::Device::Connect (ev::redis::Device::ConnectedCallback a_callback)
{
    if ( nullptr != hiredis_context_ ) {
        // ...
        connected_callback_ = a_callback;
        connected_callback_(ev::redis::Device::ConnectionStatus::Connected, this);
        // ... already connected ...
        return ev::redis::Device::Status::Nop;
    }

    if ( nullptr == event_base_ptr_ ) {
        return ev::redis::Device::Status::Error;
    }

    connected_callback_ = a_callback;

    hiredis_context_ = redisAsyncConnect(ip_address_.c_str(), port_number_);
    if ( nullptr == hiredis_context_ ) {
        connected_callback_ = nullptr;
        return ev::redis::Device::Status::OutOfMemory;
    }

    if ( REDIS_OK != redisLibeventAttach(hiredis_context_, event_base_ptr_) ) {
        connected_callback_ = nullptr;
        return ev::redis::Device::Status::Error;
    }

    if ( REDIS_OK != redisAsyncSetConnectCallback(hiredis_context_, HiredisConnectCallback) ) {
        connected_callback_ = nullptr;
        return ev::redis::Device::Status::Error;
    }

    if ( REDIS_OK != redisAsyncSetDisconnectCallback(hiredis_context_, HiredisDisconnectCallback) ) {
        connected_callback_ = nullptr;
        return ev::redis::Device::Status::Error;
    }

    hiredis_context_->data = this;

    // ... asynchronous connect ...
    return ev::redis::Device::Status::Async;
}

/**
 * @brief Close the current a REDIS connection.
 *
 * @param a_callback
 *
 * @return One of \link ev::redis::Device::Status \link.
 */
ev::redis::Device::Status ev::redis::Device::Disconnect (ev::redis::Device::DisconnectedCallback a_callback)
{
    if ( nullptr == hiredis_context_ ) {
        // ... already disconnected ...
        return ev::redis::Device::Status::Nop;
    }
    
    disconnected_callback_ = a_callback;

    redisAsyncDisconnect(hiredis_context_);

    // ... asynchronous disconnect ...
    return ev::redis::Device::Status::Async;
}

/**
 * @brief Execute a command using the current REDIS connection.
 *
 * @param a_callback
 * @param a_format
 * @param ...
 *
 * @return One of \link ev::redis::Device::Status \link.
 */
ev::redis::Device::Status ev::redis::Device::Execute (ev::redis::Device::ExecuteCallback a_callback, const ev::Request* a_request)
{
    const ev::redis::Request* redis_request = dynamic_cast<const ev::redis::Request*>(a_request);
    if ( nullptr == redis_request ) {
        // ... can't execute command ...
        return ev::redis::Device::Status::Error;
    }
    
    // ... no connection?
    if ( nullptr == hiredis_context_ ) {
        // ... can't execute command ...
        return ev::redis::Device::Status::Error;
    }
    
    ev::redis::Device::Status rv;

    execute_callback_ = a_callback;
    request_ptr_      = redis_request;
    
    const std::string& payload = redis_request->AsString();

    int async_rv;
    if ( REDIS_OK != ( async_rv = redisAsyncFormattedCommand(hiredis_context_, HiredisDataCallback, nullptr, payload.c_str(), payload.length()) ) ) {
        rv                = ev::redis::Device::Status::Error;
        execute_callback_ = nullptr;
        request_ptr_      = nullptr;
    } else {
        rv = ev::redis::Device::Status::Async;
    }

    // ... always increase reuse counter ...
    IncreaseReuseCount();

    // ... we're done ...
    return rv;    
}

/**
 * @return The last set error object, nullptr if none.
 */
ev::Error* ev::redis::Device::DetachLastError ()
{
    if ( 0 != last_error_msg_.length() ) {
        return new ev::redis::Error(last_error_msg_);
    } else {
        return nullptr;
    }
}

#ifdef __APPLE__
#pragma mark
#endif

/**
 * @brief This method is called by HIREDIS to notify a 'database selection' command reply.
 *
 * @param a_status
 * @param a_result
 */
void ev::redis::Device::DatabaseIndexSelectionCallback (const ev::redis::Device::ExecutionStatus& a_status, ev::Result* a_result)
{
    try {
        
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
                                      a_result
        );
        
        const ev::redis::Reply* reply = dynamic_cast<const ev::redis::Reply*>(a_result->DataObject());

        if ( nullptr == reply ) {
            throw ev::Exception("Unable to set REDIS database for index %d - received 'nullptr' as reply!",
                                database_index_
            );
        }
        
        const ev::redis::Value& value = reply->value();
        if ( ev::redis::Value::ContentType::Status != value.content_type() ) {
            throw ev::Exception("Unable to set REDIS database for index %d - unexpected reply content type (%d)!",
                                database_index_, static_cast<int>(value.content_type())
            );
        }
        // ... expecting 'OK' as reply ...
        if ( 0 != strcasecmp(value.String().c_str(), "OK") ) {
            throw ev::Exception("Unable to set REDIS database for index %d - unexpected status '%s'!",
                                database_index_, value.String().c_str()
            );
        }
        
        // ... now we can start sending other commands ...
        database_selected_ = true;
        
        // ... notify specific 'connect' callback?
        if ( nullptr != connected_callback_ ) {
            connected_callback_(connection_status_, this);
            connected_callback_ = nullptr;
        }
        
        // ... notify all listeners ...
        if ( nullptr != listener_ptr_ ) {
            listener_ptr_->OnConnectionStatusChanged(connection_status_, this);
        }

        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p, database_selected_ = true",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
                                      a_result
        );

    } catch (const ev::Exception& a_ev_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p, a_ev_exception = %s",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
				      a_result,
                                      a_ev_exception.what()
        );
        OSALITE_BACKTRACE();
        exception_callback_(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p, a_bad_alloc = %s",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
                                      a_result,
                                      a_bad_alloc.what()
        );
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p, a_rte = %s",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
                                      a_result,
                                      a_rte.what()
        );
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p, a_std_exception = %s",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
                                      a_result,
                                      a_std_exception.what()
        );
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data_,
                                      "[%-30s] : r_context = %p, a_status = " UINT8_FMT ", a_result = %p, ... = ...",
                                      __FUNCTION__,
                                      hiredis_context_,
                                      static_cast<uint8_t>(a_status),
                                      a_result
        );
        OSALITE_BACKTRACE();
        exception_callback_(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
    
    delete database_request_;
    database_request_ = nullptr;
    
    delete a_result;
}

#ifdef __APPLE__
#pragma mark - STATIC
#endif

/**
 * @brief This method is called by HIREDIS to notify a 'connect' event.
 *
 * @param a_context
 * @param a_status
 */
void ev::redis::Device::HiredisConnectCallback (const struct redisAsyncContext* a_context, int a_status)
{
    if ( nullptr == a_context ) {
        return;
    }

    ev::redis::Device* device = static_cast<ev::redis::Device*>(a_context->data);
    
    const ev::Loggable::Data loggable_data = ( nullptr != device->request_ptr_ ? device->request_ptr_->loggable_data_ : device->loggable_data_ );
    
    try {

        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, device = %p",
                                      __FUNCTION__,
                                      a_context,
                                      a_status,
                                      device
        );

        // ... check for error(s) ...
        if ( REDIS_OK != a_status ) {
            device->last_error_msg_ = nullptr != a_context->errstr ? a_context->errstr : std::to_string(a_status);
        } else {
            device->last_error_msg_ = "";
        }
        
        device->connection_status_ = ( 0 == device->last_error_msg_.length() ? ev::Device::ConnectionStatus::Connected : ev::Device::ConnectionStatus::Error );
        if ( ev::Device::ConnectionStatus::Connected != device->connection_status_ ) {
            device->hiredis_context_->data = nullptr;
            device->hiredis_context_ = nullptr;
        }
        
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, connection_status_ = " UINT8_FMT ", last_error_msg_ = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_status,
                                      static_cast<uint8_t>(device->connection_status_),
                                      device->last_error_msg_.c_str()
        );
        
        // ... should select a REDIS database before allowing to run any other command(s) ?
        if ( ev::Device::ConnectionStatus::Connected == device->connection_status_ && -1 != device->database_index_ && false == device->database_selected_ ) {
            
            // ... for debug proposes only ...
            ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                          "[%-30s] : a_context = %p, a_status = %d, SELECT database_index_ = %d",
                                          __FUNCTION__,
                                          a_context,
                                          a_status,
                                          device->database_index_
            );
            
            // ... yes, create a 'special' request ...
            device->database_request_ = new ev::redis::Request(loggable_data,
                                                               "SELECT", { std::to_string(device->database_index_) }
            );
            
            // ... callbacks are now differ, it will be called when this 'special' request is done ...
            const ev::redis::Device::Status select_status = device->Execute(
                                                                            std::bind(&ev::redis::Device::DatabaseIndexSelectionCallback, device, std::placeholders::_1, std::placeholders::_2),
                                                                            device->database_request_
            );
            
            // ... if request won't be executed ...
            if ( ev::redis::Device::Status::Async != select_status ) {
                // ... ww can't continue ...
                throw ev::Exception("Unable to start REDIS database selection for index %d!",
                                    device->database_index_
                );
            }
            
        } else { // ... connection error or no database to select ...
            
            // ... for debug proposes only ...
            ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                          "[%-30s] : a_context = %p, a_status = %d, CONNECTION ESTABLISHED connected_callback_ = %s",
                                          __FUNCTION__,
                                          a_context,
                                          a_status,
                                          device->connected_callback_ ? "<set>" : "<not set>"
            );
            
            // ... notify specific 'connect' callback?
            if ( nullptr != device->connected_callback_ ) {
                device->connected_callback_(device->connection_status_, device);
                device->connected_callback_ = nullptr;
            }
            // ... notify all listeners ...
            if ( nullptr != device->listener_ptr_ ) {
                device->listener_ptr_->OnConnectionStatusChanged(device->connection_status_, device);
            }
        }

    } catch (const ev::Exception& a_ev_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, a_ev_exception = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_status,
                                      a_ev_exception.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, a_bad_alloc = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_status,
                                      a_bad_alloc.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, a_rte = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_status,
                                      a_rte.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, a_std_exception = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_status,
                                      a_std_exception.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "[%-30s] : a_context = %p, a_status = %d, ... = ...",
                                      __FUNCTION__,
                                      a_context,
                                      a_status
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
}

/**
 * @brief This method is called by HIREDIS to notify a 'disconnect' event.
 *
 * @param a_context
 * @param a_status
 */
void ev::redis::Device::HiredisDisconnectCallback (const struct redisAsyncContext* a_context, int a_status)
{
    if ( nullptr == a_context ) {
        return;
    }
    
    ev::redis::Device* device = static_cast<ev::redis::Device*>(a_context->data);
    
    const ev::Loggable::Data loggable_data = ( nullptr != device->request_ptr_ ? device->request_ptr_->loggable_data_ : device->loggable_data_ );

    try {

        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, device = %p",
                                      a_context,
                                      a_status,
                                      device
        );

        // ... check for error(s) ...
        if ( REDIS_OK != a_status ) {
            // ... not disconnected?
            if ( REDIS_ERR_EOF != a_status ) {
                device->last_error_msg_ = a_context->errstr;
            }
        } else {
            // ... no errors ...
            device->last_error_msg_ = "";
        }
        
        // ... forget context ....
        device->hiredis_context_->data = nullptr;
        device->hiredis_context_ = nullptr;
        
        // ... set current status ...
        device->connection_status_ = device->hiredis_context_ == nullptr ? ev::Device::ConnectionStatus::Disconnected : ev::Device::ConnectionStatus::Error;
        
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, connection_status_ = " UINT8_FMT ", disconnected_callback_ = %s",
                                      a_context,
                                      a_status,
                                      static_cast<uint8_t>(device->connection_status_),
                                      nullptr != device->disconnected_callback_ ? "<set>" : "<not set>"
        );
        
        // ... specific callback request ...
        if ( nullptr != device->disconnected_callback_ ) {
            device->disconnected_callback_(device->connection_status_, device);
            device->disconnected_callback_ = nullptr;
        }
        
        // ... notify all listeners ...
        if ( nullptr != device->listener_ptr_ ) {
            device->listener_ptr_->OnConnectionStatusChanged(device->connection_status_, device);
        }
        
    } catch (const ev::Exception& a_ev_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, a_ev_exception = %s",
                                      a_context,
                                      a_status,
                                      a_ev_exception.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, a_bad_alloc = %s",
                                      a_context,
                                      a_status,
                                      a_bad_alloc.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, a_rte = %s",
                                      a_context,
                                      a_status,
                                      a_rte.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, a_std_exception = %s",
                                      a_context,
                                      a_status,
                                      a_std_exception.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace", loggable_data,
                                      "ev::redis::Device::HiredisDisconnectCallback(): a_context = %p, a_status = %d, ... = ...",
                                      a_context,
                                      a_status
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
    }
}

/**
 * @brief This method is called by HIREDIS deliver a command reply.
 *
 * @param a_context
 * @param a_reply
 * @param a_priv_data
 */
void ev::redis::Device::HiredisDataCallback (struct redisAsyncContext* a_context, void* a_reply, void* /* a_priv_data */)
{
    if ( nullptr == a_context ) {
        return;
    }

    ev::redis::Device* device = static_cast<ev::redis::Device*>(a_context->data);

    const ev::Loggable::Data loggable_data = ( nullptr != device->request_ptr_ ? device->request_ptr_->loggable_data_ : device->loggable_data_ );
    
    try {

        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, execute_callback_ = %s, handler_ptr_= %p",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      nullptr != device->execute_callback_ ? "<set>" : "<not set>",
                                      device->handler_ptr_
        );

        // ... if no one is waiting for a reply ...
        if ( nullptr == device->execute_callback_ && nullptr == device->handler_ptr_ ) {
            // ... we're done ...
            return;
        }
        
        const bool disconnecting = ( REDIS_DISCONNECTING == ( a_context->c.flags & REDIS_DISCONNECTING ) );
        
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, disconnecting = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      disconnecting ? "true" : "false"
        );
        
        // ... reset, and parse reply ...
        device->last_error_msg_ = "";
        
        ev::Result*                   result       = nullptr;
        ev::redis::Reply*             reply_object = nullptr;
        const struct redisReply*      reply        = static_cast<const struct redisReply*>(a_reply);
        if ( nullptr != reply ) {
            result = new ev::Result(ev::Object::Target::Redis);
            if ( ev::redis::Request::Kind::Subscription == device->request_ptr_->kind() ) {
                reply_object = new ev::redis::subscriptions::Reply(device->request_ptr_->loggable_data_, reply);
            } else {
                reply_object = new ev::redis::Reply(reply);
            }
            result->AttachDataObject(reply_object);
        } else {
            device->last_error_msg_ = ( true == disconnecting ? "DISCONNECTED" : "REDIS Reply: 'nullptr'!" );
        }
        
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, result = %p, execute_callback_ = %s, last_error_msg_ = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      result,
                                      nullptr != device->execute_callback_ ? "<set>" : "<not set>",
                                      device->last_error_msg_.c_str()
        );
        
        // ... notify caller ...
        bool ownership_transfered = false;
        // ...
        if ( nullptr != device->execute_callback_ ) {            
            auto       callback            = device->execute_callback_;
            device->execute_callback_      = nullptr;
            callback(device->last_error_msg_.length() > 0 ? ev::Device::ExecutionStatus::Error : ev::Device::ExecutionStatus::Ok, result);
            ownership_transfered = true;
        } else if ( nullptr != result && nullptr != device->handler_ptr_ ) {
            try {
                ownership_transfered = device->handler_ptr_->OnUnhandledDataObjectReceived(device, device->request_ptr_, result);
            } catch (const ev::Exception& a_ev_exception) {
                // ... if a result object is set and no one collected it ...
                if ( nullptr != result && false == ownership_transfered ) {
                    // ... release it now ...
                    delete result;
                }
                // ... rethrow exception ...
                throw a_ev_exception;
            }
        }
        
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, result = %p, ownership_transfered = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      result,
                                      ( true == ownership_transfered ? "true" : "false" )
        );
        
        // ... if a result object is set and no one collected it ...
        if ( nullptr != result && false == ownership_transfered ) {
            // ... release it now ...
            delete result;
        }

    } catch (const ev::Exception& a_ev_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, a_ev_exception = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      a_ev_exception.what()
        );
		OSALITE_BACKTRACE();
        device->last_error_msg_ = a_ev_exception.what();
        device->exception_callback_(a_ev_exception);
    } catch (const std::bad_alloc& a_bad_alloc) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, a_bad_alloc = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      a_bad_alloc.what()
        );
		OSALITE_BACKTRACE();
        device->last_error_msg_ = a_bad_alloc.what();
        device->exception_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
    } catch (const std::runtime_error& a_rte) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, a_rte = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      a_rte.what()
        );
		OSALITE_BACKTRACE();
        device->last_error_msg_ = a_rte.what();
        device->exception_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
    } catch (const std::exception& a_std_exception) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, a_std_exception = %s",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device,
                                      a_std_exception.what()
        );
        OSALITE_BACKTRACE();
        device->exception_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
    } catch (...) {
        // ... for debug proposes only ...
        ev::Logger::GetInstance().Log("redis_trace_extended", loggable_data,
                                      "[%-30s] : a_context = %p, a_reply = %p, device = %p, ... = ...",
                                      __FUNCTION__,
                                      a_context,
                                      a_reply,
                                      device
        );
		OSALITE_BACKTRACE();
        device->last_error_msg_ = STD_CPP_GENERIC_EXCEPTION_TRACE();
        device->exception_callback_(ev::Exception(device->last_error_msg_));
    }
}
