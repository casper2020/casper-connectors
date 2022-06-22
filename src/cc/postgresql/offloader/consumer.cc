/**
 * @file consumer.cc
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

#include "cc/postgresql/offloader/consumer.h"

#include "cc/threading/worker.h"

#include "cc/debug/types.h"

#include "cc/postgresql/offloader/logger.h"

// MARK: -

/**
 * @brief Default constructor.
 */
cc::postgresql::offloader::Consumer::Consumer ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    thread_          = nullptr;
    aborted_         = false;
    start_cv_        = nullptr;
    shared_ptr_      = nullptr;
    connection_      = nullptr;
    reuse_count_     = 0;
    max_reuse_count_ = -1;
    idle_start_  = std::chrono::steady_clock::now();
    CC_IF_DEBUG_SET_VAR(thread_id_, ::cc::debug::Threading::k_invalid_thread_id_;)
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::Consumer::~Consumer ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    if ( nullptr != thread_ ) {
        delete thread_;
    }
    if ( nullptr != start_cv_ ) {
        delete start_cv_;
    }
    if ( nullptr != connection_ ) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

// MARK: -

/**
 * @brief Start consumer.
 *
 * @param a_name     Parent process name or abbreviation to be used for thread name prefix.
 * @param a_listener See \link offloader::Listener \link.
 * @param a_shared   Shared data.
 */
void cc::postgresql::offloader::Consumer::Start (const std::string& a_name, offloader::Listener a_listener, offloader::Shared* a_shared)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s", __FUNCTION__);
    CC_ASSERT(nullptr == start_cv_ && nullptr == thread_);
    
    aborted_    = false;
    listener_   = a_listener;
    start_cv_   = new osal::ConditionVariable();
    shared_ptr_ = a_shared;
    thread_     = new std::thread(&cc::postgresql::offloader::Consumer::Loop, this, shared_ptr_->config().polling_timeout_ms_);
    thread_nm_  = a_name + "::pg::offloader::Consumer";
    thread_->detach();
    start_cv_->Wait();
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s", __FUNCTION__);
}

/**
 * @brief Stop consumer.
 * */
void cc::postgresql::offloader::Consumer::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s", __FUNCTION__);
    
    aborted_ = true;
    // ... consumer thread can now be release ...
    if ( nullptr != thread_ ) {
        delete thread_;
        thread_ = nullptr;
    }
    if ( nullptr != start_cv_ ) {
        delete start_cv_;
        start_cv_ = nullptr;
    }
    // ... reset ...
    shared_ptr_ = nullptr;
    if ( nullptr != connection_ ) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
    reuse_count_ = 0;
    listener_    = { nullptr, nullptr };
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s", __FUNCTION__);
}

// MARK: -

/**
 * @brief Try to cancel a query execution, if it's running.
 *
 * @param a_ticket Execution ticket, see \link offloader::Ticket \link.
 */
void cc::postgresql::offloader::Consumer::Cancel (const cc::postgresql::offloader::Ticket& a_ticket)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr != shared_ptr_);
    // ... cancel ticket ...
    shared_ptr_->Cancel(a_ticket);
    // ℹ️
    // https://www.postgresql.org/docs/11/libpq-cancel.html
    // ...
    // The return value is 1 if the cancel request was successfully dispatched and 0 if not.
    // If not, errbuf is filled with an error message explaining why not. errbuf must be a char array of size errbufsize (the recommended size is 256 bytes).
    // ...
    // PQcancel can safely be invoked from a signal handler, if the errbuf is a local variable in the signal handler.
    // The PGcancel object is read-only as far as PQcancel is concerned, so it can also be invoked from a thread that is separate from the one manipulating the PGconn object.
    //
    // ... already running?
    mutex_.lock();
    if ( 0 == exec_uuid_.compare(a_ticket.uuid_) ) {
        PGcancel* cancel = PQgetCancel(connection_);
        char cancel_error_[256];
        cancel_error_[0] = '\0';
        if ( 1 != PQcancel(cancel, cancel_error_, sizeof(cancel_error_) / sizeof(cancel_error_[0])) ) {
            // ... log ...
            CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s, " CC_LOGS_LOGGER_COLOR(RED) "%s" CC_LOGS_LOGGER_RESET_ATTRS,
                                            "CANCEL ISSUED",
                                            a_ticket.uuid_.c_str(), cancel_error_
            );
        }
        PQfreeCancel(cancel);
    }
    mutex_.unlock();
}

// MARK: -

/**
 * @brief Consumer loop.
 *
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 *
 */
void cc::postgresql::offloader::Consumer::Loop (const float& a_polling_timeout)
{
    uint64_t tmp_elapsed = 0;
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s", __FUNCTION__);
    // ... sanity set ...
    CC_IF_DEBUG_SET_VAR(thread_id_, cc::debug::Threading::GetInstance().CurrentThreadID());
    // ... name this thread ...
    ::cc::threading::Worker::SetName(thread_nm_);
    ::cc::threading::Worker::BlockSignals({SIGUSR1, SIGUSR2, SIGTTIN, SIGTERM, SIGQUIT});
    // ... partial reset ...
    reuse_count_ = 0;
    idle_start_  = std::chrono::steady_clock::now();
    // ...
    const std::set<ExecStatusType>& acceptable = { ExecStatusType::PGRES_COMMAND_OK, ExecStatusType::PGRES_TUPLES_OK };
    // ... ready ...
    start_cv_->Wake();
    // ... while not aborted ...
    while ( false == aborted_ ) {
        // ... check for pending orders ...
        bool done = false;
        while ( false == done ) {
            // ... any order(s) remaining?
            offloader::Pending pending;
            if ( true == shared_ptr_->Pull(pending) ) {
                // ... yes, process it ...
                PGresult* result = nullptr;
                try {
                    // ... execute ...
                    result = Execute(pending.uuid_, pending.query_, acceptable, tmp_elapsed);
                    // ... deliver ...
                    shared_ptr_->Pop(pending, result, tmp_elapsed);
                    // ... clean up ...
                    PQclear(result);
                } catch (const ::cc::Exception& a_exception) {
                    // ... clean up ...
                    if ( nullptr != result ) {
                        PQclear(result);
                    }
                    shared_ptr_->Pop(pending, a_exception, tmp_elapsed);
                }
            } else if ( shared_ptr_->config().idle_timeout_ms_ > 0 && nullptr != connection_ ) {
                Disconnect(/* a_idle */ true, /* a_resaon */ nullptr);
            }
            // ... dont't be CPU greedy ...
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(a_polling_timeout)));
        }
    }
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s", __FUNCTION__);
}

// MARK: -

/**
 * @brief Notify producer that an order was fulfilled.
 *
 * @param a_result Order result data.
 */
void cc::postgresql::offloader::Consumer::OnOrderFulfilled (const ::cc::postgresql::offloader::OrderResult& a_result)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr != listener_.on_performed_ && nullptr != listener_.on_failure_);
    // ... notify ...
    if ( nullptr == a_result.exception_ ) {
        listener_.on_performed_(a_result);
    } else {
        listener_.on_failure_(a_result, *a_result.exception_);
    }
}

/**
 * @brief Notify producer that an order failed.
 *
 * @param a_result    Order result data.
 * @param a_exception Exception occurred.
 */
void cc::postgresql::offloader::Consumer::OnOrderFailed (const ::cc::postgresql::offloader::OrderResult& a_result,
                                                         const ::cc::Exception& a_exception)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr != listener_.on_failure_);
    // ... notify ..
    listener_.on_failure_(a_result, a_exception);
}

/**
 * @brief Notify producer that an order was cancelled.
 *
 * @param a_order Cancelled pending order info.
 */
void cc::postgresql::offloader::Consumer::OnOrderCancelled (const ::cc::postgresql::offloader::PendingOrder& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr != listener_.on_cancelled_);
    // ... notify ...
    listener_.on_cancelled_(a_order);
}

// MARK: -

/**
 * @brief Ensure a connection.
 */
void cc::postgresql::offloader::Consumer::Connect ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    const bool recycle = ( nullptr != connection_ && max_reuse_count_ > 0 && reuse_count_ >= static_cast<size_t>(max_reuse_count_) );
    if ( nullptr == connection_ || CONNECTION_BAD == PQstatus(connection_) || true == recycle ) {
        // ... set reason ..
        const std::string reason = std::string(", due to ") + ( ( nullptr != connection_ && false == recycle ) ? "bad connection" : true == recycle ? "recycle" : "not being connected" );
        // ... for debug purposes only ...
        CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s()%s", __FUNCTION__,
                         reason.c_str()
        );
        // ... clean up ...
        Disconnect(/* a_idle */ false, reason.c_str());
        // ... reset ...
        const auto config = shared_ptr_->config();
        reuse_count_      = 0;
        max_reuse_count_  = config.rnd_max_queries();
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, ...",
                                        "CONNECTING"
        );
        // ... connect ...
        mutex_.lock();
        connection_ = PQconnectdb(config.str_.c_str());
        mutex_.unlock();
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, INFO, as %s to %s at %s:%s",
                                        "CONNECTION",
                                        PQuser(connection_), PQdb(connection_), PQhost(connection_), PQport(connection_)
        );
        // ... failed?
        if ( CONNECTION_BAD == PQstatus(connection_) ) {
            // ... keep track of error ...
            const std::string msg = std::string(PQerrorMessage(connection_));
            // ... clean up ...
            mutex_.lock();
            PQfinish(connection_);
            connection_ = nullptr;
            mutex_.unlock();
            // ... log ...
            CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, FAILED, CONNECTION_BAD, %s",
                                            "CONNECTION",
                                            msg.c_str()
            );
            // ... for debug purposes only ...
            CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s() - %s!", __FUNCTION__, msg.c_str());
            // ... report ...
            throw ::cc::Exception(msg);
        } else {
            // ... log ...
            CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s, max reuse set to " SSIZET_FMT ", idle timeout is " UINT64_FMT "ms and backend PID is %d",
                                            "CONNECTED",
                                            "CONNECTION_OK", max_reuse_count_, shared_ptr_->config().idle_timeout_ms_, PQbackendPID(connection_)
            );
        }
        // ... post connect setup ...
        Json::Value array = Json::Value(Json::ValueType::arrayValue);
        // ... set statement timeout?
        if ( 0 != shared_ptr_->config().statement_timeout_ ) {
            array.append(Json::Value("SET statement_timeout TO " + std::to_string(shared_ptr_->config().statement_timeout_ * 1000) + ";"));
        }
        if ( true == shared_ptr_->config().post_connect_queries_.isArray() ) {
            for ( Json::ArrayIndex idx = 0 ; idx < shared_ptr_->config().post_connect_queries_.size() ; ++idx ) {
                array.append(shared_ptr_->config().post_connect_queries_[idx]);
            }
        }
        // ... execute post connect queries ...
        const std::set<ExecStatusType>& acceptable = { ExecStatusType::PGRES_COMMAND_OK, ExecStatusType::PGRES_TUPLES_OK };
        for ( Json::ArrayIndex idx = 0 ; idx < array.size() ; ++idx ) {
            // ... next query ...
            const auto query = array[idx].asString();
            // ... log ...
            CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, " CC_LOGS_LOGGER_COLOR(CYAN) "%s" CC_LOGS_LOGGER_RESET_ATTRS,
                                            "EXECUTING", query.c_str());
            // ... execute ...
            const auto start = std::chrono::steady_clock::now();
            PGresult* result = PQexec(connection_, query.c_str());
            const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
            const auto status = PQresultStatus(result);
            // ... validate ...
            if ( acceptable.end() == acceptable.find(status) ) { // ... not acceptable ...
                // ... log ...
                CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, " CC_LOGS_LOGGER_COLOR(RED) "%s" CC_LOGS_LOGGER_RESET_ATTRS ", %s, took " UINT64_FMT "ms",
                                                "EXECUTED", PQresStatus(status), PQerrorMessage(connection_), elapsed);
            } else {
                // ... log ...
                CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, " CC_LOGS_LOGGER_COLOR(GREEN) "%s" CC_LOGS_LOGGER_RESET_ATTRS ", took " UINT64_FMT "ms",
                                                "EXECUTED", PQresStatus(status), elapsed);
            }
            // ... clean up ...
            PQclear(result);
        }
        // ... for debug purposes only ...
        CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s()", __FUNCTION__);
    }
}

/**
 * @brief Close connection, if any.
 *
 * @param a_idle   When true it will only disconnect if IDLE condition is triggered.
 * @param a_reason Motive for this function call.
 */
void cc::postgresql::offloader::Consumer::Disconnect (const bool a_idle, const char* const a_reason)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... nothing to do?
    if ( nullptr == connection_ ) {
        // ... done ...
        return;
    }
    // ... for debug purposes only ...
    bool due_to_idle = false;
    // ... check for IDLE condition?
    if ( true == a_idle ) {
        const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - idle_start_).count());
        if ( elapsed < shared_ptr_->config().idle_timeout_ms_ ) {
            // ... done ...
            return;
        }
        due_to_idle = true;
    }
    const char* const reason = due_to_idle ? ", due to idle" : ( nullptr != a_reason ? a_reason : ", ???" );
    // ... log ...
    CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, from backend w/PID %d%s",
                                    "DISCONNECTING",
                                    PQbackendPID(connection_), reason
    );
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s(a_idle=%s)", __FUNCTION__, a_idle ? "true" : "false");
    // ... clean up ...
    mutex_.lock();
    PQfinish(connection_);
    connection_ = nullptr;
    mutex_.unlock();
    // ... log ...
    CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s%s",
                                    "DISCONNECTED", reason
    );
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s(...) - disconnected%s", __FUNCTION__, reason);
}

/**
 * @brief Execute a PostgreSQL 'SELECT' query.
 *
 * @param a_uuid       Universal Unique ID.
 * @param a_query      PostgreSQL query to execute.
 * @param a_acceptable Acceptable result status.
 * @param o_elapsed    Query execution elapsed time, in milliseconds.
 */
PGresult* cc::postgresql::offloader::Consumer::Execute (const std::string& a_uuid,
                                                        const std::string& a_query, const std::set<ExecStatusType>& a_acceptable, uint64_t& o_elapsed)
{
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "~> %s(\"%s\", ...)", __FUNCTION__, a_uuid.c_str());
    // ... ensure connection ...
    Connect();
    // ... increase counter ...
    reuse_count_++;
    // ... log ...
    CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s",
                                    "PROCESSING",
                                    a_uuid.c_str()
    );
    CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, " CC_LOGS_LOGGER_COLOR(CYAN) "%s" CC_LOGS_LOGGER_RESET_ATTRS,
                                    "EXECUTING", a_query.c_str());
    // ... execute ...
    exec_uuid_ = a_uuid;
    const auto start = std::chrono::steady_clock::now();
    PGresult* result = PQexec(connection_, a_query.c_str());
    o_elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    exec_uuid_.clear();
    // ... failed?
    const auto status = PQresultStatus(result);
    if ( a_acceptable.end() == a_acceptable.find(status) ) { // ... not acceptable ...
        // ... clean up ...
        PQclear(result);
        result = nullptr;
        // ... partial reset ...
        idle_start_ = std::chrono::steady_clock::now();
        // ... for debug purposes only ...
        CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s(\"%s\", ...) - FAILURE: %s", __FUNCTION__, a_uuid.c_str(), PQerrorMessage(connection_));
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, " CC_LOGS_LOGGER_COLOR(RED) "%s" CC_LOGS_LOGGER_RESET_ATTRS ", %s, took " UINT64_FMT "ms",
                                        "EXECUTED", PQresStatus(status), PQerrorMessage(connection_), o_elapsed);
        // ... report ...
        throw ::cc::Exception(std::string(PQerrorMessage(connection_)));
    }
    // ... partial reset ...
    idle_start_ = std::chrono::steady_clock::now();
    // ... log ...
    CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, " CC_LOGS_LOGGER_COLOR(GREEN) "%s" CC_LOGS_LOGGER_RESET_ATTRS ", took " UINT64_FMT "ms",
                                    "EXECUTED", PQresStatus(status), o_elapsed);
    // ... for debug purposes only ...
    CC_DEBUG_LOG_MSG("offloader::Consumer", "<~ %s(\"%s\", ...) - took " UINT64_FMT "ms", __FUNCTION__, a_uuid.c_str(), o_elapsed);
    // ... done ..
    return result;
}
