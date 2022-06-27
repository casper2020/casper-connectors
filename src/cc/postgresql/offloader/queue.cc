/**
 * @file queue.cc
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

#include "cc/postgresql/offloader/queue.h"

#include "cc/exception.h"

#include "cc/debug/types.h"

#include "cc/postgresql/offloader/logger.h"

// MARK: -

/**
 * @brief Default constructor.
 *
 * @param a_config \link offloader::Config \link.
 */
cc::postgresql::offloader::Queue::Queue (const offloader::Config& a_config)
 : config_(a_config)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
}

/**
 * @brief Destructor.
 */
cc::postgresql::offloader::Queue::~Queue ()
{
    Reset();
}

// MARK: -

/**
 * @brief Bind listener.
 *
 * @param a_listener Listener to set ( if not null ) or unset ( if null ).
 */
void cc::postgresql::offloader::Queue::Bind (offloader::Listener a_listener)
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
void cc::postgresql::offloader::Queue::Reset ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... clean up ...
    std::lock_guard<std::mutex> lock(mutex_);    
    while ( orders_.size() > 0 ) {
        delete orders_.front();
        orders_.pop_front();
    }
    ids_.clear();
    for ( auto map : { &executed_, &cancelled_, &failed_ } ) {
        for ( auto it : *map ) {
            if ( nullptr != it.second->table_ ) {
                delete it.second->table_;
            }
            if ( nullptr != it.second->exception_ ) {
                delete it.second->exception_;
            }
            delete it.second;
        }
        map->clear();
    }
    try_to_cancel_.clear();
    listener_ = { nullptr, nullptr };
}

// MARK: -

/**
 * @brief Enqueue a query execution order.
 *
 * @param a_order Execution order, see \link offloader::Order \link..
 *
 * @return Execution ticket, see \link offloader::Ticket \link.
 */
cc::postgresql::offloader::Ticket
cc::postgresql::offloader::Queue::Enqueue (const offloader::Order& a_order)
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
        if ( ids_.end() != ids_.find(uuid) || executed_.end() != executed_.find(uuid) || cancelled_.end() != cancelled_.find(uuid) || failed_.end() != failed_.find(uuid) || try_to_cancel_.end() != try_to_cancel_.find(uuid) ) {
            throw ::cc::Exception("%s", "Offload request FAILED triggered by unlikely ( but not impossible ) UUID collision event!");
        }
        // ... keep track if this order ...
        orders_.push_back(new offloader::PendingOrder{ uuid, a_order.query_, a_order.client_ptr_, a_order.on_success_, a_order.on_failure_});
        ids_.insert(uuid);
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
            const auto iit = ids_.find(orders_.back()->uuid_);
            if ( ids_.end() != iit ) {
                ids_.erase(iit);
            }
            delete orders_.back();
            orders_.pop_back();
        }
        failure = a_cc_exception.what();
    } catch (...) {
        // .. FAILURE ...
        if ( offloader::Status::Failed != status ) {
            const auto iit = ids_.find(orders_.back()->uuid_);
            if ( ids_.end() != iit ) {
                ids_.erase(iit);
            }
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
    // ... sanity check ....
    CC_DEBUG_ASSERT(orders_.size() == ids_.size());
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
void cc::postgresql::offloader::Queue::Cancel (const cc::postgresql::offloader::Ticket& a_ticket)
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... if still present, erase it ...
    try_to_cancel_.insert(a_ticket.uuid_);
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
bool cc::postgresql::offloader::Queue::Peek (offloader::Pending& o_pending)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... first check pending cancellations ...
    (void)PurgeTryCancel("", {}, /* a_notify */ true, /* a_mutex */ nullptr);
    // ... no pending orders?
    if ( 0 == orders_.size() ) {
        // ... no ...
        return false;
    }
    // ... pick next ...
    const auto& next = orders_.front();
    // ... yes, copy next order ...
    o_pending = offloader::Pending{ next->uuid_ , next->query_, false };
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
void cc::postgresql::offloader::Queue::DequeueExecuted (const offloader::Pending& a_pending, const PGresult* a_result, const uint64_t a_elapsed)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    CC_DEBUG_ASSERT(false == a_pending.cancelled_);
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_ASSERT(orders_.size() == ids_.size());
    CC_DEBUG_ASSERT(0 == orders_.front()->uuid_.compare(a_pending.uuid_));
    // ... pick order
    auto order = orders_.front();
    // ... move to executed map ...
    executed_[orders_.front()->uuid_] = order;
    orders_.pop_back();
    const auto iit = ids_.find(order->uuid_);
    if ( ids_.end() != iit ) {
        ids_.erase(iit);
    }
    // ... post execute cancellation check ...
    if ( true == PurgeTryCancel(a_pending.uuid_, { &executed_ }, /* a_notify */ true, /* a_mutex */ nullptr) ) {
        // ... cancelled, nothing else to do here ( it was notified via cancellation callback ) ...
        return;
    }
    // ... attach result ...
    try {
        const int rows_count    = PQntuples(a_result);
        const int columns_count = PQnfields(a_result);
        order->table_ = new offloader::Table();
        order->table_->columns_.clear();
        for ( auto column = 0 ; column < columns_count ; ++column ) {
            order->table_->columns_.push_back(PQfname(a_result, column));
        }
        for ( auto row = 0 ; row < rows_count ; ++row ) {
            order->table_->rows_.push_back({});
            auto& vector = order->table_->rows_.back();
            for ( auto column = 0 ; column < columns_count ; ++column ) {
                vector.push_back(PQgetvalue(a_result, /* row */ 0, /* column */ column));
            }
        }
    } catch (...) {
        // ... re-throw ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
    // ... notify ...
    listener_.on_performed_(*order);
}

/**
 * @brief Dequeue an order due to cancellation in the middle of execution.
 *
 * @param a_pending Pending execution info, see \link offloader::Pending \link.
 * @param a_elapsed Query execution time.
 */
void cc::postgresql::offloader::Queue::DequeueCancelled (const offloader::Pending& a_pending, const uint64_t a_elapsed)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    CC_DEBUG_ASSERT(true == a_pending.cancelled_);
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_ASSERT(orders_.size() == ids_.size());
    CC_DEBUG_ASSERT(0 == orders_.front()->uuid_.compare(a_pending.uuid_));
    // ... pick order
    auto order = orders_.front();
    // ... move to cancelled map ...
    cancelled_[order->uuid_] = order;
    orders_.pop_back();
    const auto iit = ids_.find(order->uuid_);
    if ( ids_.end() != iit ) {
        ids_.erase(iit);
    }
    // ... here, cancellations notifications must be always sent ...
    listener_.on_cancelled_(*cancelled_.find(a_pending.uuid_)->second);
}

/**
 * @brief Dequeue an order.
 *
 * @param a_pending   Pending execution info, see \link offloader::Pending \link.
 * @param a_exception Exception data.
 * @param a_elapsed   Query execution time.
 */
void cc::postgresql::offloader::Queue::DequeueFailed (const offloader::Pending& a_pending, const ::cc::Exception& a_exception, const uint64_t a_elapsed)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    CC_DEBUG_ASSERT(false == a_pending.cancelled_);
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_DEBUG_ASSERT(orders_.size() == ids_.size());
    CC_DEBUG_ASSERT(0 == orders_.front()->uuid_.compare(a_pending.uuid_));
    // ... pick order
    auto order = orders_.front();
    // ... move to executed map ...
    failed_[order->uuid_] = order;
    orders_.pop_back();
    const auto iit = ids_.find(order->uuid_);
    if ( ids_.end() != iit ) {
        ids_.erase(iit);
    }
    // ... post execute cancellation check ...
    if ( true == PurgeTryCancel(a_pending.uuid_, { &failed_ }, /* a_notify */ true, /* a_mutex */ nullptr) ) {
        // ... cancelled, nothing else to do here ( it was notified via cancellation callback ) ...
        return;
    }
    // ... attach exception ...
    order->exception_ = new ::cc::Exception(a_exception);
    // ... not cancelled, notify ...
    listener_.on_failure_(*order);
}

/**
 * @brief Release aa executed order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 */
void cc::postgresql::offloader::Queue::ReleaseExecuted (const std::string& a_uuid, const std::function<void(const offloader::PendingOrder&)>& a_callback)
{
    // ... perform safety checks and pop result ...
    ReleaseExecutedOrder(a_uuid, a_callback);
}

/**
 * @brief Release a cancelled order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 */
void cc::postgresql::offloader::Queue::ReleaseCancelled (const std::string& a_uuid, const std::function<void(const offloader::PendingOrder&)>& a_callback)
{
    // ... perform safety checks and pop result ...
    ReleaseCancelledOrder(a_uuid, a_callback);
}

/**
 * @brief Release a failed order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 */
void cc::postgresql::offloader::Queue::ReleaseFailed (const std::string& a_uuid, const std::function<void(const offloader::PendingOrder&)>& a_callback)
{
    // ... perform safety checks and pop result ...
    ReleaseFailedOrder(a_uuid, a_callback);
}

// MARK: -

/**
 * @brief Safely release an executed order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 */
void cc::postgresql::offloader::Queue::ReleaseExecutedOrder (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback)
{
    // ... clean up ...
    ReleaseOrder(a_uuid, a_callback, executed_, &mutex_);
}

/**
 * @brief Safely release an executed but cancelled ( in the middle of the execution ) order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 */
void cc::postgresql::offloader::Queue::ReleaseCancelledOrder (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback)
{
    // ... clean up ...
    ReleaseOrder(a_uuid, a_callback, cancelled_, &mutex_);
}

/**
 * @brief Safely release an executed buy failed order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 */
void cc::postgresql::offloader::Queue::ReleaseFailedOrder (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback)
{
    // ... clean up ...
    ReleaseOrder(a_uuid, a_callback, failed_, &mutex_);
}

/**
 * @brief Safely release an order object.
 *
 * @param a_uuid     Universal Unique ID.
 * @param a_callback Function to call before deleting object.
 * @param a_map      Orders map.
 * @param a_mutex    When not null, mutext to use.
 */
void cc::postgresql::offloader::Queue::ReleaseOrder (const std::string& a_uuid, const std::function<void(const PendingOrder&)>& a_callback,
                                                      std::map<std::string, PendingOrder*>& a_map, std::mutex* a_mutex) const
{
    // ... shared data safety insurance ...
    if ( nullptr != a_mutex ) {
        a_mutex->lock();
    }
    // ...
    const auto r_it = a_map.find(a_uuid);
    if ( a_map.end() == r_it ) { // ... possibly cancelled ...
        // ... gone ...
    } else {// ... NOT cancelled ...
        // ... no, deliver result ...
        try {
            a_callback(*r_it->second);
        } catch (...) {
            // ... this can not or should not happen ...
            // ... eat it or throw a fatal exception ...
        }
        // ... release ...
        try {
            // ... clean up ...
            if ( nullptr != r_it->second->table_ ) {
                delete r_it->second->table_;
            }
            if ( nullptr != r_it->second->exception_ ) {
                delete r_it->second->exception_;
            }
            delete r_it->second;
            // ... forget it ...
            a_map.erase(r_it);
        } catch (...) {
            // ... this can not or should not happen ...
            // ... eat it or throw a fatal exception ...
        }
    }
    // ... shared data safety insurance ...
    if ( nullptr != a_mutex ) {
        a_mutex->unlock();
    }
}

/**
 * @brief Safely forget orders marked to be cancelled.
 *
 * @param a_uuid   Universal Unique ID.
 * @param a_map    Orders map.
 * @param a_notify When true cancellations will be notified.
 * @param a_mutex  When not null, mutext to use.
 *
 * @return True if provided UUI was cancelled, false otherwise.
 */
bool cc::postgresql::offloader::Queue::PurgeTryCancel (const std::string& a_uuid, const std::vector<std::map<std::string, PendingOrder*>*>& a_maps,
                                                       const bool a_notify, std::mutex* a_mutex)
{
    // ... shared data safety insurance ...
    if ( nullptr != a_mutex ) {
        a_mutex->lock();
    }
    // ... sanity check ...
    CC_DEBUG_ASSERT(orders_.size() == ids_.size());
    // ... purge pre-exec cancelled order(s) ...
    bool rv = ( try_to_cancel_.end() != try_to_cancel_.find(a_uuid) );
    try {
        // ... first from orders queue ...
        std::deque<PendingOrder*> tmp;
        while ( orders_.size() > 0 ) {
            auto order = orders_.front();
            if ( try_to_cancel_.end() != try_to_cancel_.find(order->uuid_) ) {
                // ... notify?
                if ( true == a_notify ) {
                    // ... yes ...
                    listener_.on_cancelled_(*order);
                }
                // ... clean up ...
                delete order;
            } else {
                tmp.push_back(order);
            }
            orders_.pop_front();
        }
        const auto iit = ids_.find(a_uuid);
        if ( ids_.end() != iit ) {
            ids_.erase(iit);
        }
        // ... move back ...
        while ( tmp.size() > 0 ) {
            orders_.push_back(tmp.front());
            tmp.pop_front();
        }
        // ... now for provided map(s) ...
        for ( const auto& uuid : try_to_cancel_ ) {
            for ( auto map : a_maps ) {
                // ... present?
                const auto it = map->find(uuid);
                if ( map->end() == it ) {
                    // .. no ...
                    continue;
                }
                // ... yes, release it ...
                try {
                    // ... clean up ...
                    if ( nullptr != it->second->table_ ) {
                        delete it->second->table_;
                    }
                    if ( nullptr != it->second->exception_ ) {
                        delete it->second->exception_;
                    }
                    delete it->second;
                    // ... forget it ...
                    map->erase(it);
                } catch (...) {
                    // ... this can not or should not happen ...
                    // ... eat it or throw a fatal exception ...
                    rv = false;
                }
            }
        }
        // ... clean up ...
        try_to_cancel_.clear();
    } catch (...) {
        // ... this can not or should not happen ...
        // ... eat it or throw a fatal exception ...
        rv = false;
    }
    // ... shared data safety insurance ...
    if ( nullptr != a_mutex ) {
        a_mutex->unlock();
    }
    // ... done ...
    return rv;
}
