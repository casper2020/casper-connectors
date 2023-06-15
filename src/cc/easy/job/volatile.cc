/**
 * @file volatile.h
 *
 * Copyright (c) 2011-2023 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/easy/job/volatile.h"

#include "ev/redis/request.h"
#include "ev/redis/value.h"
#include "ev/redis/reply.h"

#include "cc/easy/json.h"
#include "cc/easy/beanstalk.h"

/**
 * @brief Default constructor.
 */
cc::easy::job::Volatile::Volatile (const ev::beanstalk::Config& a_config, const ev::Loggable::Data& a_loggable_data)
 : config_(a_config), loggable_data_(a_loggable_data)
{
    ::ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Destructor.
 */
cc::easy::job::Volatile::~Volatile ()
{
    ::ev::scheduler::Scheduler::GetInstance().Unregister(this);
    for ( const auto& it : producers_ ) {
        delete it.second;
    }
}

// MARK: -

/**
 * @brief One-shot call only.
 */
void cc::easy::job::Volatile::Setup ()
{
    // ... sanity check ...
    CC_ASSERT(0 == producers_.size());
    for ( const auto& tube : config_.tubes_ ) {
        producers_[tube] = new ::ev::beanstalk::Producer(config_, tube);
    }
}

// MARK: -

/**
 * @brief Submit a job but do not wait for it's return ( or response ).
 *
 * @param a_job    Pre-filled job data.
 * @param o_result Submition result !!! NOT !!! job result.
 * @param a_cv     External 'condition variable' to unlock once it's finished.
 */
void cc::easy::job::Volatile::Submit (const Volatile::Job& a_job, Volatile::Result& o_result,
                                      osal::ConditionVariable& a_cv)
{
    // ... submit to beanstalk ...
    ReserveAndPush(a_job, o_result, a_cv);
}

// MARK: -

/**
 * @brief Reserve a JOB ID and push a job to beanstalkd.
 *
 * @param a_job    Pre-filled job data.
 * @param o_result Submition result !!! NOT !!! job result.
 */
void cc::easy::job::Volatile::ReserveAndPush (const Job& a_job, Result& o_result, osal::ConditionVariable& a_cv)
{
    const std::string seq_id_key = a_job.service_id_ + ":jobs:sequential_id";

    o_result.key_     = ( a_job.service_id_ + ":jobs:" + a_job.tube_ + ':' );
    o_result.channel_ = ( a_job.service_id_ + ':' + a_job.tube_ + ':' );
    o_result.sc_      = 400;
    
    NewTask([this, seq_id_key] () -> ::ev::Object* {
        
        // ...  get new job id ...
        return new ::ev::redis::Request(loggable_data_, "INCR", {
            /* key   */ seq_id_key
        });
        
    })->Then([this, &o_result] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // INCR:
        //
        // - An integer reply is expected:
        //
        //  - the value of key after the increment
        //
        const ::ev::redis::Value& value = ::ev::redis::Reply::EnsureIntegerReply(a_object);
        
        o_result.id_       = static_cast<uint64_t>(value.Integer());
        o_result.key_     += std::to_string(o_result.id_);
        o_result.channel_ += std::to_string(o_result.id_);
        
        // ... first, set queued status ...
        return new ::ev::redis::Request(loggable_data_, "HSET", {
            /* key   */ o_result.key_,
            /* field */ "status", "{\"status\":\"queued\"}"
        });
        
    })->Then([this, &a_job, &o_result] (::ev::Object* a_object) -> ::ev::Object* {
        
        //
        // HSET:
        //
        // - An integer reply is expected:
        //
        //  - 1 if field is a new field in the hash and value was set.
        //  - 0 if field already exists in the hash and the value was updated.
        //
        (void)::ev::redis::Reply::EnsureIntegerReply(a_object);
        
        return new ::ev::redis::Request(loggable_data_, "EXPIRE", { o_result.key_, std::to_string(a_job.expires_in_) });
        
    })->Finally([this, &a_job, &a_cv, &o_result] (::ev::Object* a_object) {
            
            //
            // EXPIRE:
            //
            // Integer reply, specifically:
            // - 1 if the timeout was set.
            // - 0 if key does not exist or the timeout could not be set.
            //
            ::ev::redis::Reply::EnsureIntegerReply(a_object, 1);
            
            //
            // DONE
            //
            o_result.sc_ = 200;
        
            //
            // now, BEANSTALK
            //
            Json::Value* payload = nullptr;
            try {
                // ... clone payload ...
                payload = new Json::Value(a_job.payload_);
                // ... and patch it ...
                (*payload)["id"]       = o_result.id_;
                (*payload)["tube"]     = a_job.tube_;
                (*payload)["validity"] = static_cast<Json::UInt64>(a_job.validity_);
                
                const cc::easy::JSON<::ev::Exception> json;
                int64_t     status;
                const auto it = producers_.find(a_job.tube_);
                if ( producers_.end() != it ) {
                    status = it->second->Put(json.Write((*payload)), /* a_priority = 0 */ 0, /* a_delay = 0 */ 0, a_job.ttr_);
                } else {
                    ::ev::beanstalk::Producer producer(config_, a_job.tube_);
                    status = producer.Put(json.Write((*payload)), /* a_priority = 0 */ 0, /* a_delay = 0 */ 0, a_job.ttr_);
                }
                if ( status < 0 ) {
                    // ... clean up ...
                    delete payload;
                    // ... notify ...
                    throw ::ev::Exception("Beanstalk producer returned with error code " INT64_FMT " - %s!",
                                          status, ev::beanstalk::Producer::ErrorCodeToString(status)
                    );
                }
            } catch (const ::Beanstalk::ConnectException& a_btc_exception) {
                // ... clean up ...
                delete payload;
                // ... notify ...
                throw ::ev::Exception("%s", a_btc_exception.what());
            }

            // RELEASE job control
            a_cv.Wake();
                    
    })->Catch([&a_cv, &o_result] (const ::ev::Exception& a_ev_exception) {
        
        o_result.sc_ = 500;
        o_result.ew_ = a_ev_exception.what();
        
        // RELEASE job control
        a_cv.Wake();
        
    });
    // ⚠️ why can't 'Wait()' here - this 'condition variable' is external - caller shall call it if / when required
}

// MARK: -

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
::ev::scheduler::Task* cc::easy::job::Volatile::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ::ev::scheduler::Task(a_callback,
                                     [this](::ev::scheduler::Task* a_task) {
                                         ::ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                     }
    );
}
