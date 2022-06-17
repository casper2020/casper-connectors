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
 * @brief Reset by releasing all previously allocated data.
 */
void cc::postgresql::offloader::Shared::Reset ()
{
    // ... sanity check ...
    CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    // ... clean up ...
    std::lock_guard<std::mutex> lock(mutex_);
    while ( orders_.size() > 0 ) {
        delete orders_.front();
        orders_.pop_front();
    }
    pending_.clear();
    cancelled_.clear();
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
            ss << tmp << std::hex << static_cast<const void*>(a_order.client_ptr_) << pending_.size();
        }
        uuid = ss.str();
        // ... avoid IDs collision ( unlikely but not impossible ) ...
        if ( pending_.end() != pending_.find(uuid) || cancelled_.end() != cancelled_.find(uuid) ) {
            throw ::cc::Exception("%s", "Offload request FAILED triggered by unlikely ( but not impossible ) UUID collision event!");
        }
        // ... keep track if this order ...
        orders_.push_back(new Shared::PendingOrder{ uuid, a_order.query_, a_order.client_ptr_ });
        pending_[uuid] = orders_.back();
        // ... accepted, mark as pending ...
        status = offloader::Status::Pending;
    } catch (const ::cc::Exception& a_cc_exception) {
        // .. FAILURE ...
        const auto it = pending_.find(uuid);
        if ( pending_.end() != it ) {
            pending_.erase(it);
            orders_.pop_back();
        }
        failure = a_cc_exception.what();
    } catch (...) {
        // .. FAILURE ...
        const auto it = pending_.find(uuid);
        if ( pending_.end() != it ) {
            pending_.erase(it);
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
            /* index_      */ static_cast<uint64_t>(pending_.size() - 1),
            /* total_      */ static_cast<uint64_t>(pending_.size()),
            /* status_     */ offloader::Status::Pending,
            /* reason      */ ""
        };
    }
    // ... FAILURE ...
    return offloader::Ticket {
        /* uuid_       */ "",
        /* index_      */ 0,
        /* total_      */ static_cast<uint64_t>(pending_.size()),
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
}

/**
 * @brief Peek next pending order.
 *
 * @param a_pending Pending execution info, see \link offloader::Pending \link.
 *
 * @return True when a pending order is available and was set, false otherwise.
 */
bool cc::postgresql::offloader::Shared::Peek (offloader::Pending& a_pending)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... first check pending cancellations ...
    Purge(/* a_unsafe */ true);
    // ... no pending orders?
    if ( 0 == pending_.size() ) {
        // ... no ...
        return false;
    }
    // ... sanity check ...
    CC_DEBUG_ASSERT(pending_.size() == orders_.size());
    // ... pick next ...
    const auto& next = orders_.front();
    // ... yes, copy next order ...
    a_pending = offloader::Pending{ next->uuid_ , next->query_ };
    // ... done ...
    return true;
}

/**
 * @brief Dequeue an order.
 *
 * @param a_pending Pending execution info, see \link offloader::Pending \link.
 * @param a_result  PostgreSQL result.
 */
void cc::postgresql::offloader::Shared::Pop (const offloader::Pending& a_pending, const PGresult* a_result)
{
    Pop(a_pending, [a_result](const PendingOrder& a_po) {
        // ... no, extract data ...
        // TODO: implement:
        Client::Table table;
        const int rows_count    = PQntuples(a_result);
        const int columns_count = PQnfields(a_result);
        table.columns_.clear();
        for ( auto column = 0 ; column < columns_count ; ++column ) {
            table.columns_.push_back(PQfname(a_result, column));
        }
        table.data_.clear();
        for ( auto row = 0 ; row < rows_count ; ++row ) {
            table.data_.push_back({});
            auto& vector = table.data_.back();
            for ( auto column = 0 ; column < columns_count ; ++column ) {
                vector.push_back(PQgetvalue(a_result, /* row */ 0, /* column */ column));
            }
        }
        // ... notify ...
        // TODO: implement
        CC_ASSERT(1 == 0);
    });
}

/**
 * @brief Dequeue an order.
 *
 * @param a_pending   Pending execution info, see \link offloader::Pending \link.
 * @param a_exception Exception data.
 */
void cc::postgresql::offloader::Shared::Pop (const offloader::Pending& a_pending, const ::cc::Exception a_exception)
{
    Pop(a_pending, [](const PendingOrder& a_po) {
        // TODO: implement
        CC_ASSERT(1 == 0);
    });
}

// MARK: -

/**
 * @brief Run through lists and purge cancelled orders.
 *
 * @param a_unsafe When true, mutex won't be used.
 */
void cc::postgresql::offloader::Shared::Purge (const bool a_unsafe)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
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
            const auto it = pending_.find(uuid);
            if ( pending_.end() != it ) {
                pending_.erase(it);
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
 * @brief Dequeue an order.
 *
 * @param a_pending   Pending execution info, see \link offloader::Pending \link.
 * @param a_exception Exception data.
 */
void cc::postgresql::offloader::Shared::Pop (const offloader::Pending& a_pending, const std::function<void(const PendingOrder&)>& a_callback)
{
    // ... sanity check ...
    CC_DEBUG_ASSERT(false == cc::debug::Threading::GetInstance().AtMainThread());
    // ... shared data safety insurance ...
    std::lock_guard<std::mutex> lock(mutex_);
    // ... sanity check ...
    CC_ASSERT(0 == orders_.back()->uuid_.compare(a_pending.uuid_));
    // ... pop it ...
    const auto target = orders_.front();
    orders_.pop_front();
    // ... cancelled?
    const auto it = pending_.find(a_pending.uuid_);
    if ( it == pending_.end()) {
        // ... yes, clean up ...
        delete target;
        // ... nothing else to do here ...
        return;
    }
    // ... no ...
    try {
        // ... process it ...
        a_callback(*target);
    } catch (const ::cc::Exception& a_exception) {
        // ... clean up ...
        delete target;
        // ... re-throw ...
        throw a_exception;
    } catch(...) {
        // ... clean up ...
        delete target;
        // ... re-throw ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
}
