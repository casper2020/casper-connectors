/**
 * @file consumer.cc
 *
 * Copyright (c) 2010-2017 Cloudware S.A. All rights reserved.
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

#include "ev/loop/beanstalkd/looper.h"

#include "ev/exception.h"

#include "osal/osalite.h"
#include "osal/condition_variable.h"

#include "json/json.h"

#include "cc/macros.h"
#include "cc/i18n/singleton.h"

#ifdef __APPLE__
    #include "sys/bsd/process.h"
#endif


/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_factory
 * @param a_callbacks
 * @param a_default_tube
 */
ev::loop::beanstalkd::Looper::Looper (const ev::Loggable::Data& a_loggable_data,
                                      const ev::loop::beanstalkd::Job::Factory& a_factory, const ev::loop::beanstalkd::Job::MessagePumpCallbacks& a_callbacks)
    : ::ev::loop::beanstalkd::Object(a_loggable_data),
       factory_(a_factory), callbacks_(a_callbacks)
{
    logger_client_->Unset(ev::LoggerV2::Client::LoggableFlags::IPAddress | ev::LoggerV2::Client::LoggableFlags::OwnerPTR);
    beanstalk_        = nullptr;
    job_ptr_          = nullptr;
    CC_IF_DEBUG_SET_VAR(thread_id_, -1);
    EV_LOOP_BEANSTALK_IF_LOG_ENABLED({
        ev::LoggerV2::GetInstance().Register(logger_client_, { "queue", "pmf" });
    });
    polling_.timeout_ = -1.0;
    polling_.set_     = false;
    pmf_.pid_         = 0;
    pmf_.limit_       = 0;
    pmf_.size_        = 0;
    pmf_.purgeable_   = 0;
    pmf_.check_       = false;
    pmf_.enforce_     = false;
    pmf_.triggered_   = false;
    fatal_.exception_ = nullptr;
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Looper::~Looper ()
{
    if ( nullptr != beanstalk_ ) {
        delete beanstalk_;
    }

    for ( auto it : cache_ ) {
        delete it.second;
    }

    job_ptr_ = nullptr;

    for ( const auto& it : deferred_ ) {
        delete it.second;
    }
    deferred_.clear();

    idle_callbacks_.mutex_.lock();
    while ( idle_callbacks_.queue_.size() > 0 ) {
        delete idle_callbacks_.queue_.top();
        idle_callbacks_.queue_.pop();
    }
    idle_callbacks_.cancelled_.clear();
    idle_callbacks_.mutex_.unlock();

    if ( nullptr != fatal_.exception_ ) {
        delete fatal_.exception_;
    }
}

/**
 * @brief Consumer loop.
 *
 * @param a_shared_config Shared config.
 * @param a_hard_abort    When true a hard quit will be performed ( exit a.s.a.p. ).
 * @param a_soft_abort    When true a soft quit will be attempted ( exit when no async job is running; @ linux will depend on systemd config ).
 *
 * @return Exit code, 0 success else an error occurred.
 */
int ev::loop::beanstalkd::Looper::Run (const ev::loop::beanstalkd::SharedConfig& a_shared_config,
                                       volatile bool& a_hard_abort, volatile bool& a_soft_abort)
{   
    Beanstalk::Job job;
    std::string    uri;
    bool           cancelled;
    bool           success;
    bool           already_ran;
    bool           deferred;
    bool           bury;
    uint16_t       http_status_code;
    
    CC_IF_DEBUG_SET_VAR(thread_id_, cc::debug::Threading::GetInstance().CurrentThreadID());
    
    // ... use mem limits?
    if ( true == a_shared_config.pmf_.enabled_ ) {
        // ... set physical memory footprint settings ...
        pmf_.pid_       = a_shared_config.pid_;
        pmf_.limit_     = a_shared_config.pmf_.limit_;
        pmf_.size_      = 0;
        pmf_.purgeable_ = 0;
        pmf_.check_     = ( 0 != pmf_.pid_ && 0 != pmf_.limit_ );
#ifdef __APPLE__
        pmf_.enforce_   = ( true == pmf_.check_ && false == sys::bsd::Process::IsProcessBeingDebugged(pmf_.pid_) );
#else
        pmf_.enforce_   = false;
#endif
        pmf_.triggered_ = false;
        // ... write to permanent log ...
        if ( true == pmf_.check_ && a_shared_config.pmf_.log_level_ >= 0 ) {
            EV_LOOP_BEANSTALK_LOG("pmf",
                                  "limit    : " SIZET_FMT " bytes // " SIZET_FMT " KB // " SIZET_FMT " MB",
                                  pmf_.limit_, ( pmf_.limit_ / 1024 ), ( ( pmf_.limit_ / 1024 ) / 1024 )
            );
        }
    }

    // ... try to connect to beanstalkd ...
    beanstalk_ = new ::ev::beanstalk::Consumer();
    beanstalk_->Connect(a_shared_config.beanstalk_,
                        {
                            /* attempt_ */
                            [this, &a_shared_config] (const uint64_t& a_attempt, const uint64_t& a_max_attempts, const float& a_timeout) {
                                // ... write to permanent log ...
                                if ( std::numeric_limits<uint64_t>::max() != a_max_attempts ) {
                                    EV_LOOP_BEANSTALK_LOG("queue",
                                                          "Attempt " UINT64_FMT " of " UINT64_FMT ", trying to connect to %s:%d, timeout in %.0f second(s)...",
                                                          a_attempt, a_max_attempts,
                                                          a_shared_config.beanstalk_.host_.c_str(),
                                                          a_shared_config.beanstalk_.port_,
                                                          a_timeout
                                    );
                                } else {
                                    EV_LOOP_BEANSTALK_LOG("queue",
                                                          "Trying to connect to %s:%d, timeout in %.0f second(s)...",
                                                          a_shared_config.beanstalk_.host_.c_str(),
                                                          a_shared_config.beanstalk_.port_,
                                                          a_timeout
                                    );
                                }
                            },
                            /* failure */
                            [this] (const uint64_t& a_attempt, const uint64_t& a_max_attempts, const std::string& a_what) {
                                // ... write to permanent log ...
                                if ( std::numeric_limits<uint64_t>::max() != a_max_attempts ) {
                                    EV_LOOP_BEANSTALK_LOG("queue",
                                                          "# " UINT64_FMT " failed: %s",
                                                          a_attempt,
                                                          a_what.c_str()
                                    );
                                } else {
                                    EV_LOOP_BEANSTALK_LOG("queue",
                                                          "Failed: %s",
                                                          a_what.c_str()
                                    );
                                }
                            }
                        },
                        a_hard_abort
    );

    // ... established or aborted?
    if ( false == a_hard_abort ) { // ... established ...
        // ... write to permanent log ...
        EV_LOOP_BEANSTALK_LOG("queue",
                              "%s",
                              "Connection established..."
        );
        std::stringstream ss;
        for ( auto it : a_shared_config.beanstalk_.tubes_ ) {
            ss << it << ',';
        }
        ss.seekp(-1, std::ios_base::end); ss << '\0';
        EV_LOOP_BEANSTALK_LOG("queue",
                              "Listening to '%s' %s...",
                              ss.str().c_str(),
                              a_shared_config.beanstalk_.tubes_.size() != 1 ? "tubes" : "tube"
        );
        // ... update logger info ...
        loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
        // ... write to permanent log ...
        EV_LOOP_BEANSTALK_LOG("queue",
                              "WTNG %19.19s"  "-",
                              "--"
        );
    } else { // ... aborted ...
        // ... write to permanent log ...
        EV_LOOP_BEANSTALK_LOG("queue",
                              "%s",
                              "Connection aborted..."
        );
    }
        
    //
    // set polling timeout
    //
    const uint32_t polling_timeout = ( true == polling_.set_ ? 0 : static_cast<uint32_t>(a_shared_config.beanstalk_.abort_polling_) );

    bool   soft_aborted  = false;
    size_t stalling_size = 0;
    
    //
    // consumer loop
    //
    while ( false == a_hard_abort && nullptr == fatal_.exception_ ) {
        
        // ...
        // ... special case: soft abort
        // ...               try to wait until all running jobs are completed
        // ...               by ignoring all tubes and waiting until there are no jobs running
        // ...
        if ( true == a_soft_abort ) {
            // ... ignore all tubes, only once ...
            if ( false == soft_aborted ) {
                // ... ignore tube ...
                beanstalk_->Ignore(a_shared_config.beanstalk_);
                soft_aborted = true;
                // ... write to permanent log ...
                EV_LOOP_BEANSTALK_LOG("queue",
                                      "%s",
                                      "Soft aborted triggered..."
                );
            }
            // ... check number of running jobs ...
            if ( 0 == deferred_.size() ) {
                // ... write to permanent log ...
                EV_LOOP_BEANSTALK_LOG("queue",
                                      "%s",
                                      "Soft abort exiting..."
                );
                // ... done ...
                break;
            } else if ( deferred_.size() != stalling_size ){
                // ... write to permanent log ...
                EV_LOOP_BEANSTALK_LOG("queue",
                                      "Soft abort is waiting for " SIZET_FMT " deferred job(s)...",
                                      deferred_.size()
                );
                size_t cnt = 0;
                for ( const auto& it : deferred_ ) {
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          " # " SIZET_FMT " - soft abort is being stalled by %s, max timeout was set to " INT64_FMT " second(s), " INT64_FMT " second(s) ago...",
                                          cnt, it.first.c_str(), it.second->timeout_,
                                          std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - it.second->started_at_).count()
                    );
                    cnt++;
                }
                stalling_size = deferred_.size();
            }
        }
        
        // ... test abort flag, every n seconds ...
        if ( false == beanstalk_->Reserve(job, polling_timeout) ) {
            // ... idle check ...
            Idle(/* a_fake */ false);
            // ... manual polling override?
            if ( true == polling_.set_ && 0 == polling_timeout ) {
                usleep(static_cast<useconds_t>(polling_.timeout_ * 1000));
            }
            // ... next job ...
            continue;
        } else {
            Idle(/* a_fake */ true);
        }
        
        loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
      
        // ... write to permanent log ...
        EV_LOOP_BEANSTALK_LOG("queue",
                              "RSRVD" INT64_FMT_MAX_RA ": %12.12s: " SIZET_FMT " byte(s)",
                              job.id(),
                              "PAYLOAD",
                              job.body().length()
        );
        
        uri              = "";
        cancelled        = false;
        success          = false;
        already_ran      = false;
        deferred         = false;
        bury             = true;
        http_status_code = 400;
        
        Json::Value job_payload = Json::Value();
        Json::Reader json_reader;
        if ( false == json_reader.parse(job.body(), job_payload) ) {
            // ... write to permanent log ...
            EV_LOOP_BEANSTALK_LOG("queue",
                                  "RSRVD" INT64_FMT_MAX_RA ": %12.12s: %s",
                                  job.id(),
                                  "PAYLOAD",
                                  job.body().c_str()
            );
            // ... log the error ...
            EV_LOOP_BEANSTALK_LOG("queue",
                                  "Job #" INT64_FMT_MAX_RA ": %12.12s: %s - %s",
                                  job.id(),
                                  "FAILURE",
                                  "An error occurred while loading job payload: JSON parsing error",
                                  json_reader.getFormattedErrorMessages().c_str()
            );
            
            // ... bury it - making it available for human inspection ....
            beanstalk_->Bury(job);
            
            // ... since a call to asString() can allocate dynamic memory ...
            // ... CLEAR this object to null to free memory now ...
            job_payload.clear();
            
            // ... perform a fake IDLE  ...
            Idle(/* a_fake */ true);
            
            // ... next job ...
            continue;
        }
        
        const std::string tube = job_payload.get("tube", "").asString();

        // check if consumer is already loaded
        const auto cached_it = cache_.find(tube);
        if ( cache_.end() == cached_it ) {
            // ... new job handler is required ...
            try {
                // ... create a new instance ..
                job_ptr_ = factory_(tube);
                if ( nullptr == job_ptr_ ) {
                    throw ev::Exception("Unknown tube named '%s'!", tube.c_str());
                }
               // ... give current job an oportunity to write a message as if it was the consumer ...
               job_ptr_->SetOwnerLogCallback([this] (const char* const a_tube, const char* const a_key, const std::string& a_value) {
                   // ... temporarly set tube as tag ...
                   loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), a_tube);
                   // ... write to permanent log ...
                   EV_LOOP_BEANSTALK_LOG("queue",
                                         "--- %-16.16s ---: %12.12s: %s",
                                         " ",
                                         a_key, a_value.c_str()
                   );
                   // ... restore ...
                   loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
               });
                // ... one-shot setup ...
                job_ptr_->Setup(&callbacks_, a_shared_config, std::bind(&Looper::OnDeferredJobFinished, this, std::placeholders::_1, std::placeholders::_2));
                // ... keep track of it ...
                cache_[tube] = job_ptr_;
            } catch (...) {
                // .. CRITICAL FAILURE ...
                try {
                    ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
                } catch (const ::cc::Exception& a_cc_exception) {
                    // ... write to permanent log ...
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA ": %12.12s: %s",
                                          job.id(),
                                          "TUBE",
                                          tube.c_str()
                    );
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA ": %12.12s: %s",
                                          job.id(),
                                          "FAILURE",
                                          "CAN'T CREATE A NEW JOB HANDLER INSTANCE"
                    );
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA ": %12.12s: %s",
                                          job.id(),
                                          "EXCEPTION",
                                          a_cc_exception.what()
                    );
                    if ( nullptr != job_ptr_ ) {
                        job_ptr_->Dismantle(&a_cc_exception);
                    }
                }
                // ... forget it  ...
                const auto it = cache_.find(tube);
                if ( cache_.end() != it ) {
                    cache_.erase(it);
                }
                if ( nullptr != job_ptr_ ) {
                    // ... untrack it ...
                    OnDeferredJobFailed(job_ptr_->ID(), job_ptr_->RJID());
                    // ... forget it  ...
                    delete job_ptr_;
                    job_ptr_ = nullptr;
                }
            }
        } else {
            job_ptr_ = cached_it->second;
        }
        
        // ... ready?
        if ( nullptr != job_ptr_ ) {
            osal::ConditionVariable job_cv;
            // ... yes, process job ...
            try {
                // ... mark start ...
                EV_LOOP_BEANSTALK_LOG("queue",
                                      "Job #" INT64_FMT_MAX_RA " ~> Run...",
                                      job.id()
                );
                // ... run it ...
                job_ptr_->Consume(/* a_id */ job.id(), /* a_body */ job_payload,
                                  /* on_completed_ */
                                  [&uri, &http_status_code, &success, &job_cv, this](const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code) {
                                      // ...
                                      uri              = a_uri;
                                      http_status_code = a_http_status_code;
                                      success          = a_success;
                                      // ... untrack it ...
                                      OnDeferredJobFinished(job_ptr_->ID(), job_ptr_->RJID());
                                      job_cv.Wake();
                                  },
                                  /* on_cancelled_ */
                                  [&cancelled, &already_ran, &job_cv, this] (bool a_already_ran) {
                                      // ...
                                      cancelled   = true;
                                      already_ran = a_already_ran;
                                      // ... untrack it ...
                                      OnDeferredJobFinished(job_ptr_->ID(), job_ptr_->RJID());
                                      // ... continue ...
                                      job_cv.Wake();
                                  },
                                  /* a_deferred_callback */
                                  [&deferred, &job_cv, this] (const int64_t& a_bjid, const std::string& a_rjid, const int64_t a_timeout) {
                                      // ...
                                      deferred = true;
                                      // ... track it ...
                                      OnJobDeferred(a_bjid, a_rjid, a_timeout);
                                      // ... continue ...
                                      job_cv.Wake();
                                  }
                );
                // ... WAIT for job completion ...
                job_cv.Wait();
                // ... check if it should be burried ...                
                bury = ( false == success && true == job_ptr_->Bury(http_status_code) );
            } catch (...) {
                // .. FAILURE ...
                try {
                    ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
                } catch (const ::cc::Exception& a_cc_exception) {
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA ": %12.12s: %s",
                                          job.id(),
                                          "ERROR",
                                          "UNHANDLED EXCEPTION WHILE EXECUTING JOB"
                    );
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA ": %12.12s: %s",
                                          job.id(),
                                          "EXCEPTION",
                                          a_cc_exception.what()
                    );
                    job_ptr_->Dismantle(&a_cc_exception);
                }
                // ... error - already logged ...
                success          = false;
                cancelled        = false;
                already_ran      = false;
                deferred         = false;
                bury             = true;
                http_status_code = 500;
                // ... untrack it ...
                OnDeferredJobFailed(job_ptr_->ID(), job_ptr_->RJID());
                // ... forget it  ...
                delete job_ptr_;
                cache_.erase(cache_.find(tube));
                // ... WAKE from job execution ...
                job_cv.Wake();
            }
            // ... reset ptr ...
            job_ptr_ = nullptr;
        } else {
            // ... no, critical error - already logged ...
            success          = false;
            cancelled        = false;
            already_ran      = false;
            deferred         = false;
            bury             = true;
            http_status_code = 500;
        }
        
        // ... check job result ...
        if ( true == success || true == cancelled || true == already_ran || true == deferred || false == bury ) {
            
            // ... write to permanent log?
            if ( false == deferred ) {
                // ... write it ...
                if ( false == bury ) {
                    const auto it = ::cc::i18n::Singleton::k_http_status_codes_map_.find(http_status_code);
                    if ( ::cc::i18n::Singleton::k_http_status_codes_map_.end() != it  ) {
                        EV_LOOP_BEANSTALK_LOG("queue",
                                              "Job #" INT64_FMT_MAX_RA " ~> %s: %s triggered...",
                                              job.id(), it->second.c_str(), "do not bury exception"
                        );
                    } else {
                        EV_LOOP_BEANSTALK_LOG("queue",
                                              "Job #" INT64_FMT_MAX_RA " ~> " UINT16_FMT ": %s triggered...",
                                              job.id(), http_status_code, "do not bury exception"
                        );
                    }
                } else {
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA " ~> %s...",
                                          job.id(), ( true == already_ran ? "Ignored" : true == cancelled ? "Cancelled" : "Done" )
                    );
                }
            } else {
                EV_LOOP_BEANSTALK_LOG("queue",
                                      "Job #" INT64_FMT_MAX_RA " ~> Deferred...",
                                      job.id()
                );
            }
            
            // ... we're done with it ...
            beanstalk_->Del(job);
        } else {
            
            // ... write to permanent log ...
            EV_LOOP_BEANSTALK_LOG("queue",
                                  "Job #" INT64_FMT_MAX_RA " ~> Buried: making it available for human inspection",
                                  job.id()
            );
            
            // ... bury it - making it available for human inspection ....
            beanstalk_->Bury(job);
        }

        // ... write to permanent log ...
        EV_LOOP_BEANSTALK_LOG("queue",
                              "WTNG %15.15s ---:",
                              " "
        );
        
        // ... since a call to asString() can allocate dynamic memory ...
        // ... CLEAR this object to null to free memory now ...
        job_payload.clear();
        
        // ... next job ...
        Idle(/* a_fake */ true);
        
#ifdef __APPLE__
        if ( true == pmf_.check_ ) {
            const ssize_t purgeable_mem = sys::bsd::Process::PurgeableVolatile(pmf_.pid_);
            if ( -1 != purgeable_mem ) {
                // ... keep track of PMF current size ...
                pmf_.purgeable_ = static_cast<size_t>(purgeable_mem);
                // ... log?
                if (  a_shared_config.pmf_.log_level_ >= 2 ) {
                    // ... write to permanent log ...
                    EV_LOOP_BEANSTALK_LOG("pmf",
                                          "purgeable: " SIZET_FMT " bytes // " SIZET_FMT " KB // " SIZET_FMT " MB",
                                          pmf_.purgeable_, ( pmf_.purgeable_ / 1024 ), ( ( pmf_.purgeable_ / 1024 ) / 1024 )
                    );
                }
                // ... limit reached?
                if ( pmf_.purgeable_ >= pmf_.limit_ ) {
                    // ... log?
                    if ( a_shared_config.pmf_.log_level_ >= 0 ) {
                        // ... write to permanent log ...
                        EV_LOOP_BEANSTALK_LOG("pmf",
                                              "triggered: " SIZET_FMT " bytes // " SIZET_FMT " KB // " SIZET_FMT " MB - %senforced",
                                              pmf_.purgeable_, ( pmf_.purgeable_ / 1024 ), ( ( pmf_.purgeable_ / 1024 ) / 1024 ),
                                              ( false == pmf_.enforce_ ? "NOT " : "" )
                        );
                    }
                    // ... enforce limit?
                    if ( true == pmf_.enforce_ ) {
                        // ... done ...
                        pmf_.triggered_ = true;
                        break;
                    }
                }
            }
        }
#endif
    }
    
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
    
    // ... write to permanent log ...
    EV_LOOP_BEANSTALK_LOG("queue",
                          "Stopped%s...",
                          ( true == pmf_.enforce_ && true == pmf_.triggered_ ? ": physical memory limit reached" : nullptr != fatal_.exception_ ? "fatal exception" : "" )
    );
    
    //
    // release allocated memory, so this function can be called again
    // ( if required, eg: restart by signal )
    //
    if ( nullptr != beanstalk_ ) {
        delete beanstalk_;
        beanstalk_ = nullptr;
    }
    
    // ... release job handlers cache ...
    for ( auto it : cache_ ) {
        it.second->Dismantle(nullptr);
        delete it.second;
    }
    cache_.clear();
    
    // ... release deferred jobs ...
    for ( const auto& it : deferred_ ) {
        delete it.second;
    }
    deferred_.clear();

    // ... clear idle callbacks ...
    idle_callbacks_.mutex_.lock();
    while ( idle_callbacks_.queue_.size() > 0 ) {
        delete idle_callbacks_.queue_.top();
        idle_callbacks_.queue_.pop();
    }
    idle_callbacks_.cancelled_.clear();
    CC_DEBUG_ASSERT(0 == idle_callbacks_.tmp_.size());
    idle_callbacks_.mutex_.unlock();
    
    // ... done ...
    return ( ( true == a_soft_abort || true == pmf_.triggered_ ) ? 254 : nullptr != fatal_.exception_ ? 253 : 0 );
}

/**
 * @brief Append a function to be called when loop is free.
 *
 * @param a_id        Callback ID.
 * @param a_callback  Function to call.
 * @param a_timeout   Timeout in milliseconds.
 * @param a_recurrent When true end_tp_ will be reset after callback.
 */
void ev::loop::beanstalkd::Looper::AppendCallback (const std::string& a_id, Looper::IdleCallback a_callback,
                                                   const size_t a_timeout, const bool a_recurrent)
{
    idle_callbacks_.mutex_.lock();
    idle_callbacks_.queue_.push(new Looper::IdleCallbackData ({
        /* a_id       */ a_id,
        /* timeout_   */ a_timeout,
        /* recurrent_ */ a_recurrent,
        /* function_  */ a_callback,
        /* end_tp_    */ std::chrono::steady_clock::now() + std::chrono::milliseconds(a_timeout)
    }));
    idle_callbacks_.mutex_.unlock();
}

/**
 * @brief Remove a callback,
 *
 * @param a_id Callback ID.
 */
void ev::loop::beanstalkd::Looper::RemoveCallback (const std::string& a_id)
{
    idle_callbacks_.mutex_.lock();
    idle_callbacks_.cancelled_.insert(a_id);
    idle_callbacks_.mutex_.unlock();
}

/**
 * @brief Take an IDLE moment to perform a set of pending callbacks.
 *
 * @param a_fake True when it's not a real idle moment.
*/
void ev::loop::beanstalkd::Looper::Idle (const bool a_fake)
{
    // ... lock, perform and 'dequeue' pending callbacks ...
    idle_callbacks_.mutex_.lock();
    
    // ... for the case where unique id is not respected, search and delete canceled duplicated queued entries ...
    {
        size_t cancelled = 0;
        CC_DEBUG_ASSERT(0 == idle_callbacks_.tmp_.size());
        if ( 0 != idle_callbacks_.cancelled_.size() ) {
            std::set<std::string> forget;
            while ( idle_callbacks_.queue_.size() > 0 ) {
                auto e = idle_callbacks_.queue_.top();
                if ( idle_callbacks_.cancelled_.end() != idle_callbacks_.cancelled_.find(e->id_.c_str()) ) {
                    forget.insert(e->id_);
                    delete e;
                    cancelled++;
                } else {
                    idle_callbacks_.tmp_.push(e);
                }
                idle_callbacks_.queue_.pop();
            }
            CC_DEBUG_ASSERT(0 == idle_callbacks_.queue_.size() || 0 == idle_callbacks_.tmp_.size());
            for ( auto id : forget ) {
                idle_callbacks_.cancelled_.erase(idle_callbacks_.cancelled_.find(id));
            }
            while ( idle_callbacks_.tmp_.size() > 0 ) {
                idle_callbacks_.queue_.push(idle_callbacks_.tmp_.top());
                idle_callbacks_.tmp_.pop();
            }
            CC_DEBUG_ASSERT(0 == idle_callbacks_.tmp_.size());
        }
    }
    
    try {
        // ... nothing to do?
        if ( 0 == idle_callbacks_.queue_.size() ) {
            // ... ⚠️ since we're only expecting to cancel previously registered callbacks ...
            // ... and we're under a mutex, we can safely forget all cancellations ...
            idle_callbacks_.cancelled_.clear();
            // ... unlock before leaving ....
            idle_callbacks_.mutex_.unlock();
            // ... we're done ...
            return;
        }
        // ... process as many as we can ...
        const auto   start         = std::chrono::steady_clock::now();
        const size_t exec_timeout = ( a_fake ? 100 : 50 );
        while ( idle_callbacks_.queue_.size() > 0 && nullptr == fatal_.exception_ ) {
            // ... pick next callback ...
            auto callback = idle_callbacks_.queue_.top();
            // ... check if it was cancelled ...
            const auto it = idle_callbacks_.cancelled_.find(callback->id_);
            if ( idle_callbacks_.cancelled_.end() != it ) {
                // ... no, forget it ( it doesn't matter if it's a recurrent or not ) ...
                idle_callbacks_.queue_.pop();
                delete callback;
                // ... foreget cancellation order ...
                idle_callbacks_.cancelled_.erase(it);
            } else {
                // ... we're done ..
                if ( callback->end_tp_ > start ) {
                    // ... because any other callback in queue has lower prority ...
                    break;
                }
                // ... perform it ...
                try {
                    // ... remove callback from queue ...
                    idle_callbacks_.queue_.pop();
                    // ... and unlock queue ...
                    idle_callbacks_.mutex_.unlock();
                    // ... execute it ...
                    callback->function_(callback->id_);
                    // ... re-lock queue ...
                    idle_callbacks_.mutex_.lock();
                    // ... if cancelled?
                    if ( idle_callbacks_.cancelled_.end() != idle_callbacks_.cancelled_.find(callback->id_) ) {
                        // ... reset recurrent flag ...
                        callback->recurrent_ = false;
                    } else if ( true == callback->recurrent_ ) {
                        // ... keep track of it by adding back to the queue ...
                        idle_callbacks_.queue_.push(callback);
                    }
                } catch (...) {
                    // ... track exception ...
                    try {
                        ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
                    } catch (const ::cc::Exception& a_cc_exception) {
                        fatal_.exception_ = new ::cc::Exception(a_cc_exception);
                    }
                }
                // ... recurrent?
                if ( true == callback->recurrent_ ) {
                    // ... yes, reschedule it ...
                    callback->end_tp_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(callback->timeout_);
                } else {
                    // ... no, forget it ...
                    delete callback;
                }
            }
            // ... check if we've time for more ...
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if ( elapsed >= exec_timeout ) {
                // ... we don't ...
                break;
            }
        }
    } catch (...) {
        if ( nullptr == fatal_.exception_ ) {
            try {
                ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
            } catch (const ::cc::Exception& a_cc_exception) {
                fatal_.exception_ = new ::cc::Exception(a_cc_exception);
            }
        }
    }
    if ( nullptr != fatal_.exception_ ) {
        EV_LOOP_BEANSTALK_LOG("queue",
                              "ERROR %s",
                              fatal_.exception_->what()
        );
    }
    // ... and unlock queue ...
    idle_callbacks_.mutex_.unlock();
}

/**
 * @brief Called when a deferred job started.
 *
 * @param a_bjid BEANSTALKD job id ( for logging purposes ).
 * @param a_rjid REDIS job key.
 * @param a_timeout Timeout, in seconds.
 */
void ev::loop::beanstalkd::Looper::OnJobDeferred (const int64_t& a_bjid, const std::string& a_rjid, const int64_t a_timeout)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    const auto it = deferred_.find(a_rjid);
    if ( deferred_.end() != it ) {
        delete it->second;
        deferred_.erase(it);
    }
    deferred_[a_rjid] = new Looper::Deferred({ /* bjid_ */ a_bjid, /* started_at_ */ std::chrono::steady_clock::now(), a_timeout });
}


/**
 * @brief Called when a deferred job fails to start.
 *
 * @param a_bjid BEANSTALKD job id ( for logging purposes ).
 * @param a_rjid REDIS job key.
 */
void ev::loop::beanstalkd::Looper::OnDeferredJobFailed (const int64_t& a_bjid, const std::string& a_rjid)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    const auto it = deferred_.find(a_rjid);
    if ( deferred_.end() != it ) {
        delete it->second;
        deferred_.erase(it);
    }
}

/**
 * @brief Called when a deferred job execution is now finished.
 *
 * @param a_bjid BEANSTALKD job id ( for logging purposes ).
 * @param a_rjid REDIS job key.
 */
void ev::loop::beanstalkd::Looper::OnDeferredJobFinished (const int64_t& a_bjid, const std::string& a_rjid)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    const auto it = deferred_.find(a_rjid);
    if ( deferred_.end() != it ) {
        delete it->second;
        deferred_.erase(it);
    }
}
