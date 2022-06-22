/**
 * @file shared.cc
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

#include "cc/postgresql/offloader/shared.h"

#include "cc/exception.h"

#include "cc/debug/types.h"

#include "cc/postgresql/offloader/logger.h"

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_config \link offloader::Config \link.
 */
cc::postgresql::offloader::Shared::Shared (const offloader::Config& a_config)
 : config_(a_config)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::Shared::~Shared ()
{
    Reset();
}

// MARK: -

/**
 * @brief Bind listener.
 *
 * @param a_listener Listener to set ( if not null ) or unset ( if null ).
 */
void cc::postgresql::offloader::Shared::Bind (offloader::Listener a_listener)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... there can only be one ...
    CC_ASSERT(nullptr == listener_.on_performed_ && nullptr == listener_.on_cancelled_);
    // ... set / unset ...
    listener_ = a_listener;
}

/**
 * @brief Reset by releasing all previously allocated data.
 */
void cc::postgresql::offloader::Shared::Reset ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // TODO: check caller
    // ... clean up ...
    std::lock_guard<std::mutex> lock(mutex_);
    
    while ( orders_.size() > 0 ) {
        delete orders_.front();
        orders_.pop_front();
    }
    
    for ( auto ti : executed_ ) {
        delete ti.second;
    }
    executed_.clear();
    
    cancelled_.clear();
    listener_ = { nullptr, nullptr };
}

// MARK: -

/**
 * @brief Queue an query execution order.
 *
 * @param a_order Execution order, see \link offloader::Order \link..
 *
 * @return Execution ticket, see \link offloader::Ticket \link.
 */
cc::postgresql::offloader::Ticket
cc::postgresql::offloader::Shared::Queue (const offloader::Order& a_order)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    //
    std::string uuid, failure;
    offloader::Status  status = offloader::Status::Failed;
    try {
        // ... generate an unique ID ...
        std::stringstream ss;
        {
            char tmp[37];
            uuid_t tmp_uuid; uuid_generate_random(tmp_uuid); uuid_unparse(tmp_uuid, tmp);
            ss << tmp << '-' << std::hex << std::uppercase << static_cast<const void*>(a_order.client_ptr_) << '-' << orders_.size();
        }
        uuid = ss.str();
        // ... avoid IDs collision ( unlikely but not impossible ) ...
        if ( executed_.end() != executed_.find(uuid) || cancelled_.end() != cancelled_.find(uuid) || results_.end() != results_.find(uuid) ) {
            throw ::cc::Exception("%s", "Offload request FAILED triggered by unlikely ( but not impossible ) UUID collision event!");
        }
        // ... keep track if this order ...
        orders_.push_back(new offloader::PendingOrder{ uuid, a_order.query_, a_order.client_ptr_, a_order.on_success_, a_order.on_failure_});
        // ... accepted, mark as pending ...
        status = offloader::Status::Pending;
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s",
                                        "QUEUED",
                                        uuid.c_str()
        );
    } catch (const ::cc::Exception& a_cc_exception) {
        // .. FAILURE ...
        if ( offloader::Status::Failed != status ) {
            delete orders_.back();
            orders_.pop_back();
        }
        failure = a_cc_exception.what();
    } catch (...) {
        // .. FAILURE ...
        if ( offloader::Status::Failed != status ) {
            delete orders_.back();
            orders_.pop_back();
        }
        // ... report ...
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::Exception& a_cc_exception) {
            failure = a_cc_exception.what();
        } catch (...)  {
            failure = "???";
        }
    }
    // ... should't be possible to reach here ...
    if ( offloader::Status::Pending == status ) {
        return offloader::Ticket {
            /* uuid_       */ uuid,
            /* index_      */ static_cast<uint64_t>(orders_.size() - 1),
            /* total_      */ static_cast<uint64_t>(orders_.size()),
            /* status_     */ offloader::Status::Pending,
            /* reason      */ ""
        };
    }
    // ... FAILURE ...
    return offloader::Ticket {
        /* uuid_       */ "",
        /* index_      */ 0,
        /* total_      */ static_cast<uint64_t>(orders_.size()),
        /* status_     */ status,
        /* reason      */ failure,
    };
}

/**
 * @brief Try to cancel a query execution.
 *
 * @param a_ticket Execution ticket, see \link offloader::Ticket \link.
 */
void cc::postgresql::offloader::Shared::Cancel (const cc::postgresql::offloader::Ticket& a_ticket)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... if still present, erase it ...
    cancelled_.insert(a_ticket.uuid_);
    // ... log ...
    CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s",
                                    "TRY CANCEL",
                                    a_ticket.uuid_.c_str()
    );
}

/**
 * @brief Peek next pending order.
 *
 * @param o_pending Pending execution info to set, see \link offloader::Pending \link.
 *
 * @return True when a pending order is available and was set, false otherwise.
 */
bool cc::postgresql::offloader::Shared::Pull (offloader::Pending& o_pending)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... first check pending cancellations ...
    Purge(/* a_unsafe */ true);
    // ... no pending orders?
    if ( 0 == orders_.size() ) {
        // ... no ...
        return false;
    }
    // ... pick next ...
    const auto& next = orders_.front();
    // ... yes, copy next order ...
    o_pending = offloader::Pending{ next->uuid_ , next->query_ };
    // ... move to executed step ...
    executed_[next->uuid_] = next;
    orders_.pop_back();
    // ... done ...
    return true;
}

/**
 * @brief Dequeue an order.
 *
 * @param a_pending Pending execution info, see \link offloader::Pending \link.
 * @param a_result  PostgreSQL result.
 * @param a_elapsed Query execution time.
 */
void cc::postgresql::offloader::Shared::Pop (const offloader::Pending& a_pending, const PGresult* a_result, const uint64_t a_elapsed)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    // ... perform safety checks and dequeue order ...
    SafePop(a_pending, [this, a_result, a_elapsed](const PendingOrder& a_po) {
        offloader::OrderResult* result;
        try {
            result = new offloader::OrderResult{ a_po.uuid_, a_po.query_, a_po.client_ptr_, nullptr, nullptr, a_po.on_success_, a_po.on_failure_, a_elapsed };
            const int rows_count    = PQntuples(a_result);
            const int columns_count = PQnfields(a_result);
            result->table_ = new offloader::Table();
            result->table_->columns_.clear();
            for ( auto column = 0 ; column < columns_count ; ++column ) {
                result->table_->columns_.push_back(PQfname(a_result, column));
            }
            for ( auto row = 0 ; row < rows_count ; ++row ) {
                result->table_->rows_.push_back({});
                auto& vector = result->table_->rows_.back();
                for ( auto column = 0 ; column < columns_count ; ++column ) {
                    vector.push_back(PQgetvalue(a_result, /* row */ 0, /* column */ column));
                }
            }
            // ... track ...
            const auto* ptr = result;
            results_[result->uuid_] = result;
            result = nullptr;
            // ... notify ...
            listener_.on_performed_(*ptr);
        } catch (...) {
            // ... clean up ...
            if ( nullptr != result ) {
                delete result;
            }
            // ... re-throw ...
            ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
        }
    });
}

/**
 * @brief Dequeue an order.
 *
 * @param a_pending   Pending execution info, see \link offloader::Pending \link.
 * @param a_exception Exception data.
 * @param a_elapsed   Query execution time.
 */
void cc::postgresql::offloader::Shared::Pop (const offloader::Pending& a_pending, const ::cc::Exception& a_exception, const uint64_t a_elapsed)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    // ... perform safety checks and dequeue order ...
    SafePop(a_pending, [this, &a_exception, a_elapsed](const PendingOrder& a_po) {
        offloader::OrderResult* result = new offloader::OrderResult{ a_po.uuid_, a_po.query_, a_po.client_ptr_, nullptr, nullptr, a_po.on_success_, a_po.on_failure_, a_elapsed };
        // ... mark ...
        result->exception_ = new cc::Exception(a_exception);
        // ... track ...
        results_[result->uuid_] = result;
        // ... notify ...
        listener_.on_performed_(*result);
    });
}

/**
 * @brief Release a result object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting result object.
 */
void cc::postgresql::offloader::Shared::Pop (const std::string& a_uuid, const std::function<void(const offloader::OrderResult&)>& a_callback)
{
    // ... perform safety checks and pop result ...
    SafePop(a_uuid, a_callback);
}

// MARK: -

/**
 * @brief Run through lists and purge cancelled orders.
 *
 * @param a_unsafe When true, mutex won't be used.
 */
void cc::postgresql::offloader::Shared::Purge (const bool a_unsafe)
{
    try {
        // ... apply shared data safety insurance?
        if ( false == a_unsafe ) {
            mutex_.lock();
        }
        // ... for each cancelled UUID ...
        for ( const auto& uuid : cancelled_ ) {
            // TODO: check order
            // ... delete cancelled and rewrite queue ...
            std::deque<PendingOrder*> tmp;
            while ( orders_.size() > 0 ) {
                auto order = orders_.front();
                if ( cancelled_.end() != cancelled_.find(order->uuid_) ) {
                    // ... notify ...
                    listener_.on_cancelled_(*order);
                    // ... clean up ...
                    delete order;
                } else {
                    tmp.push_back(order);
                }
                orders_.pop_front();
            }
            // ... move back ...
            while ( tmp.size() > 0 ) {
                orders_.push_back(tmp.front());
                tmp.pop_front();
            }
            // ... clean up ...
            const auto it = executed_.find(uuid);
            if ( executed_.end() != it ) {
                // ... notify ...
                listener_.on_cancelled_(*it->second);
                // ... clean up ...
                executed_.erase(it);
            }
        }
        // ... clean up ...
        cancelled_.clear();
        // ..unlock?
        if ( false == a_unsafe ) {
            mutex_.unlock();
        }
    } catch (...) { // ... unlikely but not impossible ...
        // ..unlock?
        if ( false == a_unsafe ) {
            mutex_.unlock();
        }
        // ... re-throw ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
}

/**
 * @brief Safely dequeue an order.
 *
 * @param a_pending   Pending execution info, see \link offloader::Pending \link.
 * @param a_exception Exception data.
 */
void cc::postgresql::offloader::Shared::SafePop (const offloader::Pending& a_pending, const std::function<void(const PendingOrder&)>& a_callback)
{
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... first check pending cancellations ...
    Purge(/* a_unsafe */ true);
    // ... cancelled?
    const auto e_it = executed_.find(a_pending.uuid_);
    if ( executed_.end() == e_it ) { // ... cancelled ....
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s",
                                        "CANCELLED",
                                        a_pending.uuid_.c_str()
        );
        // ... nothing else to do here ...
        return;
    }
    // ... no ...
    try {
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s",
                                        "DELIVERING",
                                        e_it->second->uuid_.c_str()
        );
        // ... process it ...
        a_callback(*e_it->second);
        // ... log ...
        CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s",
                                        "DELIVERED",
                                        e_it->second->uuid_.c_str()
        );
        // ... clean up ...
        delete e_it->second;
        executed_.erase(e_it);
    } catch (...) {
        // ... log ...
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::Exception& a_exception) {
            // ... log ...
            CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s, %s",
                                            "DELETED",
                                            e_it->second->uuid_.c_str(), a_exception.what()
            );
            // ... clean up ...
            delete e_it->second;
            executed_.erase(e_it);
            // ... re-throw ...
            throw a_exception;
        } catch (...) {
            // ... log ...
            CC_POSTGRESQL_OFFLOADER_LOG_MSG("libpq-offloader", "%-20.20s, %s, %s",
                                            "DELETED",
                                            e_it->second->uuid_.c_str(), "???"
            );
            // ... clean up ...
            delete e_it->second;
            executed_.erase(e_it);
            // ... re-throw ...
            ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
        }
    }
}

/**
 * @brief Safely release a result object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting result object.
 */
void cc::postgresql::offloader::Shared::SafePop (const std::string& a_uuid, const std::function<void(const OrderResult&)>& a_callback)
{
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... first check pending cancellations ...
    Purge(/* a_unsafe */ true);
    // ...
    const auto r_it = results_.find(a_uuid);
    if ( results_.end() == r_it ) { // ... possibly cancelled ...
        // ... already reported ...
    } else {// ... NOT cancelled ...
        // ... no, deliver result ...
        try {
            a_callback(*r_it->second);
        } catch (...) {
            // ... this can not or should not happen ...
            // ... eat it or throw a fatal exception ...
        }
        // ... clean up ...
        if ( nullptr != r_it->second->exception_ ) {
            delete r_it->second->exception_;
        }
        delete r_it->second;
        results_.erase(r_it);
    }
}
