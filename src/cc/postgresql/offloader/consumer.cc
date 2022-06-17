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

// MARK: -

/**
 * @brief Default constructor.
 */
cc::postgresql::offloader::Consumer::Consumer ()
{
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
 * @param a_shared          Shared data.
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 */
void cc::postgresql::offloader::Consumer::Start (offloader::Shared* a_shared, const float& a_polling_timeout)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    CC_ASSERT(nullptr == start_cv_ && nullptr == thread_);
    
    aborted_    = false;
    start_cv_   = new osal::ConditionVariable();
    shared_ptr_ = a_shared;
    thread_     = new std::thread(&cc::postgresql::offloader::Consumer::Loop, this, a_polling_timeout);
    thread_->detach();
    
    start_cv_->Wait();
}

/**
 * @brief Stop consumer.
 * */
void cc::postgresql::offloader::Consumer::Stop ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();

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
    // ... sanity set ...
    CC_IF_DEBUG_SET_VAR(thread_id_, cc::debug::Threading::GetInstance().CurrentThreadID());
    // ... name this thread ...
    ::cc::threading::Worker::SetName(std::string("TODO: name_") + "::offloader::Consumer");
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
            if ( true == shared_ptr_->Peek(pending) ) {
                // ... yes, process it ...
                PGresult* result = nullptr;
                try {
                    // ... execute ...
                    result = Execute(pending.query_, acceptable);
                    // ... deliver ...
                    shared_ptr_->Pop(pending, /* a_result */ result);
                    // ... clean up ...
                    PQclear(result);
                } catch (const ::cc::Exception& a_exception) {
                    // ... clean up ...
                    if ( nullptr != result ) {
                        PQclear(result);
                    }
                    shared_ptr_->Pop(pending, a_exception);
                }
            } else {
                Disconnect(/* a_idle */ true);
            }
            // ... dont't be CPU greedy ...
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(a_polling_timeout)));
        }
    }
}

// MARK: -

/**
 * @brief Ensure a connection.
 */
void cc::postgresql::offloader::Consumer::Connect ()
{
    if ( nullptr == connection_ || CONNECTION_BAD == PQstatus(connection_) || true == ( max_reuse_count_ > 0 &&  reuse_count_ >= static_cast<size_t>(max_reuse_count_) ) ) {
        // ... clean up ...
        Disconnect(/* a_idle */ false);
        // ... connect ...
        const auto config = shared_ptr_->config();
        reuse_count_     = 0;
        max_reuse_count_ = config.rnd_max_queries();
        connection_      = PQconnectdb(config.str_.c_str());
        if ( CONNECTION_BAD == PQstatus(connection_) ) {
            // ... keep track of error ...
            const std::string msg = std::string(PQerrorMessage(connection_));
            // ... clean up ...
            PQfinish(connection_);
            connection_ = nullptr;
            // ... report ...
            throw ::cc::Exception(msg);
        }
    }
}

/**
 * @brief Close connection, if any.
 *
 * @param a_idle When true it will only disconnect if IDLE condition is triggered.
 */
void cc::postgresql::offloader::Consumer::Disconnect (const bool a_idle)
{
    // ... check for IDLE condition?
    if ( true == a_idle ) {
        const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - idle_start_).count());
        if ( elapsed < shared_ptr_->config().idle_timeout_ms_ ) {
            // ... done ...
            return;
        }
    }
    // ... clean up ...
    if ( nullptr != connection_ ) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

/**
 * @brief Execute a PostgreSQL 'SELECT' query.
 *
 * @param a_query      PostgreSQL query to execute.
 * @param a_acceptable Acceptable result status.
 */
PGresult* cc::postgresql::offloader::Consumer::Execute (const std::string& a_query, const std::set<ExecStatusType>& a_acceptable)
{
    // ... ensure connection ...
    Connect();
    // ... increase counter ...
    reuse_count_++;
    // ... execute ...
    PGresult* result = PQexec(connection_, a_query.c_str());
    // ... failed?
    const auto status = PQresultStatus(result);
    if ( a_acceptable.end() == a_acceptable.find(status) ) {
        // ... clean up ...
        PQclear(result);
        result = nullptr;
        // ... partial reset ...
        idle_start_ = std::chrono::steady_clock::now();
        // ... report ...
        throw ::cc::Exception(std::string(PQerrorMessage(connection_)));
    }
    // ... partial reset ...
    idle_start_ = std::chrono::steady_clock::now();
    // ... done ..
    return result;
}
