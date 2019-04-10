/**
 * @file device.cc - PostgreSQL
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

// https://www.postgresql.org/docs/9.4/static/libpq-async.html

#include "ev/postgresql/device.h"

#include "ev/postgresql/request.h"
#include "ev/postgresql/object.h"
#include "ev/postgresql/reply.h"
#include "ev/postgresql/error.h"

#include "ev/exception.h"

#include "ev/logger.h"

#include "osal/osalite.h"

#include <sstream> // std::stringstream
#ifdef __APPLE__
    #include <netinet/tcp.h>
#else // ... assuming linux ...
  #include <stdlib.h>
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <netinet/tcp.h>
#endif

#ifndef EV_POSTGRESQL_DEVICE_LOG_FMT
#define EV_POSTGRESQL_DEVICE_LOG_FMT "%s, %s"
#endif

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_conn_str
 * @param a_statement_timeout
 * @param a_post_connect_queries
 * @param a_max_queries_per_conn
 */
ev::postgresql::Device::Device (const ::ev::Loggable::Data& a_loggable_data,
                                const char* const a_conn_str, const int a_statement_timeout,
                                const Json::Value& a_post_connect_queries, const ssize_t a_max_queries_per_conn)
    : ev::Device(a_loggable_data)
{
    context_                      = nullptr;
    connection_string_            = a_conn_str;
    statement_timeout_            = a_statement_timeout;
    post_connect_queries_         = a_post_connect_queries;
    post_connect_queries_applied_ = false;
    max_reuse_count_              = a_max_queries_per_conn;
}

/**
 * @brief Destructor.
 */
ev::postgresql::Device::~Device ()
{
    connected_callback_    = nullptr;
    disconnected_callback_ = nullptr;
    execute_callback_      = nullptr;
    listener_ptr_          = nullptr;
    Disconnect();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Connect to a PostgreSQL database.
 *
 * @return One of \link ev::postgresql::Device::Status \link.
 */
ev::postgresql::Device::Status ev::postgresql::Device::Connect (ev::postgresql::Device::ConnectedCallback a_callback)
{
    // ... device is not properly setup ...
    if ( nullptr == event_base_ptr_ ) {
        return ev::postgresql::Device::Status::Error;
    }

    // ... already connected?
    if ( nullptr != context_ ) {

        // ... reset flags ...
        const int del_rc = event_del(context_->event_);
        if ( 0 != del_rc ) {
            exception_callback_(ev::Exception("Error while deleting PostgreSQL event: code %d!", del_rc));
        }
        const int assign_rv = event_assign(context_->event_, event_base_ptr_, PQsocket(context_->connection_), EV_WRITE | EV_READ | EV_PERSIST, PostgreSQLEVCallback, context_);
        if ( 0 != assign_rv ) {
            exception_callback_(ev::Exception("Error while assigning PostgreSQL event: code %d!", assign_rv));
        }
        const int add_rv = event_add(context_->event_, nullptr);
        if ( 0 != add_rv ) {
            exception_callback_(ev::Exception("Error while adding PostgreSQL event: code %d!", del_rc));
        }
        
        // ... keep track of callback ...
        connected_callback_ = a_callback;
        
        // ... it will be an asynchronous call ...
        return ev::postgresql::Device::Status::Async;
    }
    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                  EV_POSTGRESQL_DEVICE_LOG_FMT " setting up a new async connection...",
                                  __FUNCTION__, "STATUS"
    );

    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                  EV_POSTGRESQL_DEVICE_LOG_FMT " setting max reuse count to %u %s...",
                                  __FUNCTION__, "STATUS",
                                  static_cast<unsigned>(max_reuse_count_),
                                  ( max_reuse_count_ == 1 ? "query" : "queries" )
    );

    int       opt_val = 0;
    const int opt_len = sizeof(opt_val);
    
    // ... a new connection is required ...
    connected_callback_      = a_callback;
    context_                 = new PostgreSQLContext(this);
    context_->connection_    = PQconnectStart(connection_string_.c_str());
    context_->loggable_data_ = loggable_data_;
    if ( nullptr == context_->connection_ ) {
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }

    // ... set non blocking ...
    if ( -1 == PQsetnonblocking(context_->connection_, 1) ) {
        last_error_msg_ = PQerrorMessage(context_->connection_);
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
    // ... grab socket ...
    auto fd = PQsocket(context_->connection_);
    if ( fd <= 0 ) {
        last_error_msg_ = PQerrorMessage(context_->connection_);
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
    // ... set keep alive ...
    opt_val = 1;
    if ( 0 != setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt_val, opt_len) ) {
        last_error_msg_ = "Unable to set PostgreSQL socket keep alive!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
#ifdef __APPLE__
    
    opt_val = 1;
    if ( 0 != setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &opt_val, opt_len) ) {
        last_error_msg_ = "Unable to set PostgreSQL socket TCP keep alive!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
#else // ... assuming linux ...
    
    // ... set time (in seconds) the connection needs to remain idle
    // before TCP starts sending keepalive probes ...
    opt_val = context_->connection_timeout_.tv_sec;
    if ( 0 != setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &opt_val, opt_len) ) {
        last_error_msg_ = "Unable to set PostgreSQL socket keep alive IDLE!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }

    // ... set time (in seconds) between individual keepalive probes ...
    opt_val = context_->connection_timeout_.tv_sec;
    if ( 0 != setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &opt_val, opt_len) ) {
        last_error_msg_ = "Unable to set PostgreSQL socket keep alive IDLE!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
    // ... maximum number of keepalive probes TCP should send before
    // dropping the connection ...
    opt_val = 1;
    if ( 0 != setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &opt_val, opt_len) ) {
        last_error_msg_ = "Unable to set PostgreSQL socket keep alive IDLE!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
#endif
    
    context_->event_ = event_new(event_base_ptr_,
                                 fd, EV_READ | EV_WRITE | EV_PERSIST, PostgreSQLEVCallback, context_);
    if ( nullptr == context_->event_ ) {
        last_error_msg_ = "Unable to create a new event for PostgreSQL socket!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
    const int add_rc = event_add(context_->event_, &context_->connection_timeout_);
    if ( 0 != add_rc ) {
        last_error_msg_ = "Unable to add PostgreSQL socket to event loop!";
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
    if ( CONNECTION_STARTED != PQstatus(context_->connection_) ) {
        last_error_msg_ = PQerrorMessage(context_->connection_);
        PQfinish(context_->connection_);
        delete context_;
        context_ = nullptr;
        connected_callback_ = nullptr;
        return ev::postgresql::Device::Status::Error;
    }
    
    last_error_msg_ = "";
    
    context_->connection_scheduled_tp_ = std::chrono::steady_clock::now();

    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                  EV_POSTGRESQL_DEVICE_LOG_FMT " asynchronous connection scheduled, context is %p...",
                                  __FUNCTION__, "STATUS",
                                  context_
    );
    
    return ev::postgresql::Device::Status::Async;
}

/**
 * @brief Close the current a PostgreSQL connection.
 *
 * @param a_callback
 *
 * @return One of \link ev::postgresql::Device::Status \link.
 */
ev::postgresql::Device::Status ev::postgresql::Device::Disconnect (ev::postgresql::Device::DisconnectedCallback a_callback)
{
    // ... not connected?
    if ( nullptr == context_ ) {
        return ev::postgresql::Device::Status::Nop;
    }
    // ... keep track of callback ...
    disconnected_callback_ = a_callback;
    // ... common disconnect ...
    Disconnect();
    // ... done ...
    return ev::postgresql::Device::Status::Nop;
}

/**
 * @brief Execute a command using the current PostgreSQL connection.
 *
 * @param a_callback
 * @param a_format
 * @param ...
 *
 * @return One of \link ev::postgresql::Device::Status \link.
 */
ev::postgresql::Device::Status ev::postgresql::Device::Execute (ev::postgresql::Device::ExecuteCallback a_callback, const ev::Request* a_request)
{
    const ev::postgresql::Request* postgresql_request = dynamic_cast<const ev::postgresql::Request*>(a_request);
    if ( nullptr == postgresql_request ) {
        // ... can't execute command ...
        return ev::postgresql::Device::Status::Error;
    }
    
    // ... no connection?
    if ( nullptr == context_ ) {
        // ... can't execute command ...
        return ev::postgresql::Device::Status::Error;
    }
    
    ev::postgresql::Device::Status rv;
    
    execute_callback_        = a_callback;
    context_->query_         = postgresql_request->AsCString();
    context_->loggable_data_ = postgresql_request->loggable_data_;
    context_->exec_start_    = std::chrono::steady_clock::now();

    // ... send the query ...
    if ( 1 != PQsendQuery(context_->connection_, postgresql_request->AsCString()) ) {
        last_error_msg_   = PQerrorMessage(context_->connection_);
        rv                = ev::postgresql::Device::Status::Error;
        execute_callback_ = nullptr;
        ev::Logger::GetInstance().Log("libpq", context_->loggable_data_,
                                      EV_POSTGRESQL_DEVICE_LOG_FMT " - %s\n\t%s",
                                      __FUNCTION__, "ERROR",
                                      last_error_msg_.c_str(),
                                      context_->query_.c_str()
        );
    } else {
        rv = ev::postgresql::Device::Status::Async;
        ev::Logger::GetInstance().Log("libpq", context_->loggable_data_,
                                      EV_POSTGRESQL_DEVICE_LOG_FMT "\n\t%s",
                                      __FUNCTION__, "SENT",
                                      context_->query_.c_str()
        );
    }

    // ... always increase reuse counter ...
    IncreaseReuseCount();

    // ... we're done ...
    return rv;
}

/**
 * @return The last set error object, nullptr if none.
 */
ev::Error* ev::postgresql::Device::DetachLastError ()
{
    if ( 0 != last_error_msg_.length() ) {
        return new ev::postgresql::Error(last_error_msg_);
    } else {
        return nullptr;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Release current database connection.
 */
void ev::postgresql::Device::Disconnect ()
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
    
    std::string reason;
    if ( true == invalidate_reuse_ && ( reuse_count_ < max_reuse_count_ ) ) {
        reason = " due to invalidation by signal";
    } else if ( reuse_count_ < max_reuse_count_ ) {
        reason = " due to invalidation by max reuse counter";
    };
    
    std::stringstream ss;
    ss << '[' << context_ << ']';
    
    const std::string log_msg_prefix = ss.str();

    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                  EV_POSTGRESQL_DEVICE_LOG_FMT " %s %u %s performed...",
                                  __FUNCTION__, "STATUS",
                                  log_msg_prefix.c_str(),
                                  static_cast<unsigned>(reuse_count_), ( reuse_count_ == 1 ? "query" : "queries" )
    );

    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                  EV_POSTGRESQL_DEVICE_LOG_FMT " %s disconnecting%s...",
                                  __FUNCTION__, "STATUS",
                                  log_msg_prefix.c_str(),
                                  reason.c_str()
   );

    context_->connection_finished_tp_ = std::chrono::steady_clock::now();

    const int kept_alive_for_n_seconnds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(context_->connection_finished_tp_ - context_->connection_established_tp_).count());
   
    // ... remove event and notify ...
    try {
        const int del_rc = event_del(context_->event_);
        if ( 0 != del_rc ) {
            exception_callback_(ev::Exception("Error while deleting PostgreSQL event: code %d!", del_rc));
        }
        // ... release connection ...
        if ( nullptr != context_->connection_ ) {
            PQfinish(context_->connection_);
            delete context_;
            context_ = nullptr;
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
            ev::Result* result = new ev::Result(ev::Object::Target::PostgreSQL);
            ev::Error*  error  = DetachLastError();
            if ( nullptr != error ) {
                result->AttachDataObject(error);
            } else {
                result->AttachDataObject(new ev::postgresql::Error("Disconnected from PostgreSQL server!"));
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
        if ( nullptr != listener_ptr_ ) {
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
    
    // ... write to permanent log ...
    if ( last_error_msg_.length() > 0 ) {
        ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                      EV_POSTGRESQL_DEVICE_LOG_FMT " %s disconnected, connection kept active for %d second(s) - %s",
                                      __FUNCTION__, "STATUS",
                                      log_msg_prefix.c_str(),
                                      kept_alive_for_n_seconnds,
                                      last_error_msg_.c_str()
        );
    } else {
        ev::Logger::GetInstance().Log("libpq-connections", loggable_data_,
                                      EV_POSTGRESQL_DEVICE_LOG_FMT " %s disconnected, connection kept active for %d second(s)",
                                      __FUNCTION__, "STATUS",
                                      log_msg_prefix.c_str(),
                                      kept_alive_for_n_seconnds
        );
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief
 *
 * @param a_fd
 * @param a_flags
 * @param a_arg
 */
void ev::postgresql::Device::PostgreSQLEVCallback (evutil_socket_t /* a_fd */, short a_flags, void* a_arg)
{
    PostgreSQLContext*      context = static_cast<PostgreSQLContext*>(a_arg);
    ev::postgresql::Device* device  = static_cast<ev::postgresql::Device*>(context->device_ptr_);
    
    PostgresPollingStatusType polling_status_type = PQconnectPoll(context->connection_);
    
    if ( PGRES_POLLING_READING == polling_status_type || PGRES_POLLING_WRITING == polling_status_type ) {
        /*
         * ( From ngx_postgres module. )
         *
         * Fix for Linux issue found by chaoslawful (via agentzh):
         *
         * "According to the source of libpq (around fe-connect.c:1215), during
         *  the state switch from CONNECTION_STARTED to CONNECTION_MADE, there's
         *  no socket read/write operations (just a plain getsockopt call and a
         *  getsockname call). Therefore, for edge-triggered event model, we
         *  have to call PQconnectPoll one more time (immediately) when we see
         *  CONNECTION_MADE is returned, or we're very likely to wait for a
         *  writable event that has already appeared and will never appear
         *  again :)"
         */
        if ( CONNECTION_MADE == PQstatus(context->connection_) && EV_WRITE == ( a_flags & EV_WRITE ) ) {
            polling_status_type = PQconnectPoll(context->connection_);
            if ( PGRES_POLLING_READING == polling_status_type || PGRES_POLLING_WRITING == polling_status_type ) {
                // ... event is always set ...
                return;
            }
        }
    }
    
    std::stringstream ss;
    ss << '[' << context << ']';
    
    const std::string log_msg_prefix = ss.str();
    
    bool call_connection_callback = false;

    const ConnStatusType connection_status = PQstatus(context->connection_);
    switch ( connection_status ) {
        case CONNECTION_NEEDED:
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_NEEDED";
            break;
        case CONNECTION_STARTED:
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_STARTED";
            break;
        case CONNECTION_OK:
        case CONNECTION_MADE:
            // .. track status ...
            context->last_connection_status_ = ( CONNECTION_MADE == connection_status ? "CONNECTION_MADE" : "CONNECTION_OK");
            // ... notify?
            if ( nullptr != device->connected_callback_ ) {
                context->connection_established_tp_ = std::chrono::steady_clock::now();
                call_connection_callback = true;
            }
            break;
        case CONNECTION_AWAITING_RESPONSE:
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_AWAITING_RESPONSE";
            break;
        case CONNECTION_AUTH_OK:
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_AUTH_OK";
            break;
        case CONNECTION_SETENV:
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_SETENV";
            break;
        case CONNECTION_SSL_STARTUP:
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_SSL_STARTUP";
            break;
        case CONNECTION_BAD:
            // ... track error ...
            device->last_error_msg_ = PQerrorMessage(context->connection_);
            // .. track status ...
            context->last_connection_status_ = "CONNECTION_BAD";
            // ... write to permanent log ...
            ev::Logger::GetInstance().Log("libpq-connections", context->loggable_data_,
                                          EV_POSTGRESQL_DEVICE_LOG_FMT " %s %s: %s",
                                          __FUNCTION__, "CONTEXT",
                                          log_msg_prefix.c_str(),
                                          context->last_connection_status_.c_str(),
                                          device->last_error_msg_.c_str()
            );
            // ... disconnect now ...
            device->Disconnect();
            return;
        default:
            // .. track status ...
            context->last_connection_status_ = "???";
            device->last_error_msg_ = "Unexpected status: " + std::to_string((int)connection_status);
            // ... write to permanent log ...
            ev::Logger::GetInstance().Log("libpq-connections", context->loggable_data_,
                                          EV_POSTGRESQL_DEVICE_LOG_FMT " %s %s: %s",
                                          __FUNCTION__, "CONTEXT",
                                          log_msg_prefix.c_str(),
                                          context->last_connection_status_.c_str(),
                                          device->last_error_msg_.c_str()
            );
            // ... disconnect now ...
            device->Disconnect();
            return ;
    }
    
    // ... write to permanent log ...
    if ( 0 != strcasecmp(context->last_connection_status_.c_str(), context->last_reported_connection_status_.c_str()) ) {
        if ( 0 != device->last_error_msg_.length() ) {
            ev::Logger::GetInstance().Log("libpq-connections", context->loggable_data_,
                                          EV_POSTGRESQL_DEVICE_LOG_FMT " %s %s: %s",
                                          __FUNCTION__, "CONTEXT",
                                          log_msg_prefix.c_str(),
                                          context->last_connection_status_.c_str(),
                                          device->last_error_msg_.c_str()
           );
        } else {
            ev::Logger::GetInstance().Log("libpq-connections", context->loggable_data_,
                                          EV_POSTGRESQL_DEVICE_LOG_FMT " %s %s",
                                          __FUNCTION__, "CONTEXT",
                                          log_msg_prefix.c_str(),
                                          context->last_connection_status_.c_str()
          );
        }
        context->last_reported_connection_status_ = context->last_connection_status_;
    }
    
    // ...
    if ( PGRES_POLLING_OK != polling_status_type ) {
        // ... not ready yet!
        return;
    }

    // ... read in all data that is currently waiting for us ...
    const int c_rv = PQconsumeInput(context->connection_);
    if ( 1 != c_rv ) {
        // .. returns 0 if there was some kind of trouble ...
        if ( 0 == c_rv ) {
            // ... keep track of error message ...
            device->last_error_msg_ = PQerrorMessage(context->connection_);
            // ... disconnect device ...
            device->Disconnect();
            // ... nothing else to do ...
            return;
        }
    }
    
    // ... this must be always called ...
    if ( 0 != PQisBusy(context->connection_) ) {
        // ... connection would block, so we wait for more data to arrive ...
        return;
    }

    // ... so far, no errors ...
    device->last_error_msg_ = "";

    // ... connection event?
    if ( true == call_connection_callback ) {
        // ... write to permanent log ...
        // ... connection established ...
        if ( device->statement_timeout_ > -1 && false == device->context_->statement_timeout_set_ ) {
            
            // ... write to permanent log ...
            ev::Logger::GetInstance().Log("libpq-connections", context->loggable_data_,
                                          EV_POSTGRESQL_DEVICE_LOG_FMT " %s setting statement timeout",
                                          __FUNCTION__, "STATUS",
                                          log_msg_prefix.c_str()
            );

            ExecStatusType post_connect_query_exec_status = PGRES_FATAL_ERROR;
            // ... set statement timeout ...
            const std::string query = "SET statement_timeout TO " + std::to_string((device->statement_timeout_ * 1000)) + ";";
            PGresult* postgresql_smt_result = PQexec(context->connection_, query.c_str());
            if ( nullptr != postgresql_smt_result ) {
                post_connect_query_exec_status = PQresultStatus(postgresql_smt_result);
                if ( PGRES_COMMAND_OK == post_connect_query_exec_status ) {
                    device->context_->statement_timeout_set_ = true;
                }
                PQclear(postgresql_smt_result);
            }
            // ...
            if ( PGRES_COMMAND_OK != post_connect_query_exec_status ) {
                device->exception_callback_(ev::Exception("Error while setting PostgreSQL statement timeout: %s!", PQresStatus(post_connect_query_exec_status)));
                return;
            }
        }
        // ... post connect queries ...
        if ( false == device->post_connect_queries_applied_  ) {
            try {
                if ( false == device->post_connect_queries_.isNull() ) {
                    // ... for all array elements ...
                    for ( Json::ArrayIndex idx = 0 ; idx < device->post_connect_queries_.size() ; ++idx ) {
                        // ... grab query ...
                        const std::string& query = device->post_connect_queries_[idx].asString();
                        // ... write to permanent log ...
                        ev::Logger::GetInstance().Log("libpq-connections", context->loggable_data_,
                                                      EV_POSTGRESQL_DEVICE_LOG_FMT " %s executing post connect query %s",
                                                      __FUNCTION__, "STATUS",
                                                      log_msg_prefix.c_str(),
                                                      query.c_str()
                        );
                        // ... synchronously execute ...
                        PGresult* post_connect_query_result = PQexec(context->connection_, query.c_str());
                        if ( nullptr != post_connect_query_result ) {
                            ExecStatusType post_connect_query_exec_status = PQresultStatus(post_connect_query_result);
                            PQclear(post_connect_query_result);
                            if ( not ( PGRES_COMMAND_OK == post_connect_query_exec_status || PGRES_TUPLES_OK == post_connect_query_exec_status ) ) {
                                device->exception_callback_(ev::Exception("Error while executing %s: %s!",
                                                                          query.c_str(),
                                                                          PQresStatus(post_connect_query_exec_status)
                                                                         )
                                                            );
                                return;
                            }
                        }
                    }
                }
                device->post_connect_queries_applied_ = true;
            } catch (const Json::Exception& a_json_exception) {
                device->exception_callback_(ev::Exception("%s", a_json_exception.what()));
                return;
            }
        }
        // ... notify ...
        device->connected_callback_(ev::Device::ConnectionStatus::Connected, device);
        device->connected_callback_ = nullptr;
    } else if ( nullptr != device->execute_callback_ ) {

        // ... alloc result ...
        if ( nullptr == device->context_->pending_result_ ) {
            device->context_->pending_result_ = new ev::Result(ev::Object::Target::PostgreSQL);
        }

        bool finished = true;

        // ... data event?
        PGresult* postgresql_result;
        while ( nullptr != ( postgresql_result = PQgetResult(context->connection_) ) ) {
            try {

                // ... this must be always called ...
                if ( nullptr == postgresql_result && 0 != PQisBusy(context->connection_) ) {
                    // ... connection would block, so we wait for more data to arrive ...
                    finished = false;
                    break;
                }
                
                const ExecStatusType result_status = PQresultStatus(postgresql_result);
                
                const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - device->context_->exec_start_).count());

                ev::Logger::GetInstance().Log("libpq", device->context_->loggable_data_,
                                              EV_POSTGRESQL_DEVICE_LOG_FMT ", %ums\n\t%s",
                                              __FUNCTION__, device->GetExecStatusTypeString(result_status).c_str(), static_cast<unsigned>(elapsed),
                                              device->context_->query_.c_str()
                );
                
                if ( ( PGRES_COMMAND_OK != result_status ) && ( PGRES_TUPLES_OK != result_status ) ) {
                    // ... failed ...
                    device->context_->pending_result_->AttachDataObject(new ev::postgresql::Reply(result_status, PQresStatus(result_status), elapsed));
                } else {
                    // ... succeeded ....
                    device->context_->pending_result_->AttachDataObject(new ev::postgresql::Reply(postgresql_result, elapsed));
                }
                
                postgresql_result = nullptr;
                
            } catch (const ev::Exception& a_ev_exception) {
                OSALITE_BACKTRACE();
                device->last_error_msg_ = a_ev_exception.what();
                device->exception_callback_(a_ev_exception);
            } catch (const std::bad_alloc& a_bad_alloc) {
                OSALITE_BACKTRACE();
                device->last_error_msg_ = a_bad_alloc.what();
                device->exception_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
            } catch (const std::runtime_error& a_rte) {
                OSALITE_BACKTRACE();
                device->last_error_msg_ = a_rte.what();
                device->exception_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
            } catch (const std::exception& a_std_exception) {
                OSALITE_BACKTRACE();
                device->last_error_msg_ = a_std_exception.what();
                device->exception_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
            } catch (...) {
                OSALITE_BACKTRACE();
                device->last_error_msg_ = STD_CPP_GENERIC_EXCEPTION_TRACE();
                device->exception_callback_(ev::Exception(device->last_error_msg_));
            }
            // ... if postgresql_result ownership was not transfered ...
            if ( nullptr != postgresql_result ) {
                // ... release it now ...
                PQclear(postgresql_result);
            }
            
        }
        
        const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - device->context_->exec_start_).count());

        // ... no errors?
        if ( 0 == device->last_error_msg_.length() ) {
            // ... if we're not finished yet ...
            if ( false == finished ) {
                ev::Logger::GetInstance().Log("libpq", device->context_->loggable_data_,
                                              EV_POSTGRESQL_DEVICE_LOG_FMT ", %ums - %s\n\t%s",
                                              __FUNCTION__, "WAITING", elapsed,
                                              "more results on their way...",
                                              device->context_->query_.c_str()
                );
                return;
            } else if ( 0 == device->context_->pending_result_->DataObjectsCount() ) {
                // ... busy?
                if ( 0 != PQisBusy(context->connection_) ) {
                    ev::Logger::GetInstance().Log("libpq", device->context_->loggable_data_,
                                                  EV_POSTGRESQL_DEVICE_LOG_FMT ", %ums - %s\n\t%s",
                                                  __FUNCTION__, "PQisBusy", elapsed,
                                                  "no result objects returned...",
                                                  device->context_->query_.c_str()
                    );
                    // ... connection would block, so we wait for more data to arrive ...
                    return;
                } else {
                    // ... not busy ... proceeed and error should be set by owner ...
                    ev::Logger::GetInstance().Log("libpq", device->context_->loggable_data_,
                                                  EV_POSTGRESQL_DEVICE_LOG_FMT ", %ums -%s\n\t%s",
                                                  __FUNCTION__, "???", elapsed,
                                                  "no result objects returned...",
                                                  device->context_->query_.c_str()
                    );
                }
            }
        } else {
            ev::Logger::GetInstance().Log("libpq", device->context_->loggable_data_,
                                          EV_POSTGRESQL_DEVICE_LOG_FMT ", %ums - %s\n\t%s",
                                          __FUNCTION__, "ERROR", elapsed,
                                          device->last_error_msg_.c_str(),
                                          device->context_->query_.c_str()
            );
        }
        
        // ... notify caller ...
        device->execute_callback_(device->last_error_msg_.length() > 0 ? ev::Device::ExecutionStatus::Error : ev::Device::ExecutionStatus::Ok, device->context_->pending_result_);
        device->execute_callback_ = nullptr;
        device->context_->pending_result_ = nullptr;
        device->context_->query_  = "";
        if ( true == device->Tracked() ) {
            // ... remove WRITE flag ....
            const int del_rc = event_del(device->context_->event_);
            if ( 0 != del_rc ) {
                device->exception_callback_(ev::Exception("Error while deleting PostgreSQL event: code %d!", del_rc));
            }
            const int assign_rv = event_assign(device->context_->event_, device->event_base_ptr_, PQsocket(device->context_->connection_), EV_READ | EV_PERSIST, PostgreSQLEVCallback, device->context_);
            if ( 0 != assign_rv ) {
                device->exception_callback_(ev::Exception("Error while assigning PostgreSQL event: code %d!", assign_rv));
            }
            const int add_rv = event_add(device->context_->event_, nullptr);
            if ( 0 != add_rv ) {
                device->exception_callback_(ev::Exception("Error while adding PostgreSQL event: code %d!", add_rv));
            }
        } else {
            delete device;
        }
    } else {
        device->last_error_msg_ = "Unexpected callback!";
        device->Disconnect();
    }

}
