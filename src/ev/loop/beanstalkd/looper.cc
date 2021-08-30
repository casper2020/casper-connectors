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
    beanstalk_ = nullptr;
    job_ptr_   = nullptr;
    EV_LOOP_BEANSTALK_IF_LOG_ENABLED({
        ev::LoggerV2::GetInstance().Register(logger_client_, { "queue", "pmf" });
    });
    polling_.timeout_    = -1.0;
    polling_.set_        = false;
    phys_mem_.pid_       = 0;
    phys_mem_.limit_     = 0;
    phys_mem_.size_      = 0;
    phys_mem_.check_     = false;
    phys_mem_.enforce_   = false;
    phys_mem_.triggered_ = false;
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
    while ( idle_callbacks_.queue_.size() > 0 ) {
        delete idle_callbacks_.queue_.top();
    }
    idle_callbacks_.cancelled_.clear();
}

/**
 * @brief Consumer loop.
 *
 * @param a_shared_config
 * @param a_aborted
 *
 * @return Exit code, 0 success else an error occurred.
 */
int ev::loop::beanstalkd::Looper::Run (const ev::loop::beanstalkd::SharedConfig& a_shared_config,
                                       volatile bool& a_aborted)
{   
    Beanstalk::Job job;
    std::string    uri;
    bool           cancelled;
    bool           success;
    bool           already_ran;
    bool           deferred;
    uint16_t       http_status_code;
    
    // ... use mem limits?
    if ( true == a_shared_config.pmf_.enabled_ ) {
        // ... set physical memory footprint settings ...
        phys_mem_.pid_       = a_shared_config.pid_;
        phys_mem_.limit_     = a_shared_config.pmf_.limit_;
        phys_mem_.size_      = 0;
        phys_mem_.check_     = ( 0 != phys_mem_.pid_ && 0 != phys_mem_.limit_ );
        phys_mem_.enforce_   = ( true == phys_mem_.check_ && false == sys::bsd::Process::IsProcessBeingDebugged(phys_mem_.pid_) );
        phys_mem_.triggered_ = false;
        // ... write to permanent log ...
        if ( true == phys_mem_.check_ && a_shared_config.pmf_.log_level_ >= 0 ) {
            EV_LOOP_BEANSTALK_LOG("pmf",
                                  "limit    : " SIZET_FMT " bytes // " SIZET_FMT " KB // " SIZET_FMT " MB",
                                  phys_mem_.limit_, ( phys_mem_.limit_ / 1024 ), ( ( phys_mem_.limit_ / 1024 ) / 1024 )
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
                        a_aborted
    );

    // ... established or aborted?
    if ( false == a_aborted ) { // ... established ...
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

    //
    // consumer loop
    //
    while ( false == a_aborted ) {
        
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
        
        uri          = "";
        cancelled    = false;
        success      = false;
        already_ran  = false;
        deferred     = false;
        
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
                job_ptr_->Setup(&callbacks_, a_shared_config);
                // ... keep track of it ...
                cache_[tube] = job_ptr_;
            } catch (...) {
                // .. CRITICAL FAILURE ...
                try {
                    ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
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
                                  /* on_completed_           */
                                  [&uri, &http_status_code, &success, &job_cv](const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code) {
                                      uri              = a_uri;
                                      http_status_code = a_http_status_code;
                                      success          = a_success;
                                      job_cv.Wake();
                                  },
                                  /* on_cancelled_           */
                                  [&cancelled, &already_ran, &job_cv] (bool a_already_ran) {
                                      cancelled   = true;
                                      already_ran = a_already_ran;
                                      job_cv.Wake();
                                  },
                                  /* a_deferred_callback */
                                  [&deferred, &job_cv] () {
                                      deferred = true;
                                      job_cv.Wake();
                                  }
                );
                // ... WAIT for job completion ...
                job_cv.Wait();
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
                http_status_code = 500;
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
            http_status_code = 500;
        }
        
        loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
        
        const bool bury = ( a_shared_config.dnbe_.end() == std::find(a_shared_config.dnbe_.begin(), a_shared_config.dnbe_.end(), http_status_code) );

        // ... check print result ...
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
        if ( true == phys_mem_.check_ ) {
            const ssize_t mem_used = sys::bsd::Process::MemPhysicalFootprint(phys_mem_.pid_);
            if ( -1 != mem_used ) {
                // ... keep track of PMF current size ...
                phys_mem_.size_ = static_cast<size_t>(mem_used);
                // ... log?
                if (  a_shared_config.pmf_.log_level_ >= 2 ) {
                    // ... write to permanent log ...
                    EV_LOOP_BEANSTALK_LOG("pmf",
                                          "used     : " SIZET_FMT " bytes // " SIZET_FMT " KB // " SIZET_FMT " MB",
                                          phys_mem_.size_, ( phys_mem_.size_ / 1024 ), ( ( phys_mem_.size_ / 1024 ) / 1024 )
                    );
                }
                // ... limit reached?
                if ( phys_mem_.size_ >= phys_mem_.limit_ ) {
                    // ... log?
                    if ( a_shared_config.pmf_.log_level_ >= 0 ) {
                        // ... write to permanent log ...
                        EV_LOOP_BEANSTALK_LOG("pmf",
                                              "triggered: " SIZET_FMT " bytes // " SIZET_FMT " KB // " SIZET_FMT " MB - %senforced",
                                              phys_mem_.size_, ( phys_mem_.size_ / 1024 ), ( ( phys_mem_.size_ / 1024 ) / 1024 ),
                                              ( false == phys_mem_.enforce_ ? "NOT " : "" )
                        );
                    }
                    // ... enforce limit?
                    if ( true == phys_mem_.enforce_ ) {
                        // ... done ...
                        phys_mem_.triggered_ = true;
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
                          ( true == phys_mem_.enforce_ && true == phys_mem_.triggered_ ? ": physical memory limit reached" : "")
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
    
    // ... done ...
    return ( true == phys_mem_.triggered_ ? 254 : 0 );
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
    std::lock_guard<std::mutex>(idle_callbacks_.mutex_);
    idle_callbacks_.queue_.push(new Looper::IdleCallbackData ({
        /* a_id       */ a_id,
        /* timeout_   */ a_timeout,
        /* recurrent_ */ a_recurrent,
        /* function_  */ a_callback,
        /* end_tp_    */ std::chrono::steady_clock::now() + std::chrono::milliseconds(a_timeout)
    }));
}

/**
 * @brief Remove a callback,
 *
 * @param a_id Callback ID.
 */
void ev::loop::beanstalkd::Looper::RemoveCallback (const std::string& a_id)
{
    std::lock_guard<std::mutex>(idle_callbacks_.mutex_);
    idle_callbacks_.cancelled_.insert(a_id);
}

/**
 * @brief Take an IDLE moment to perform a set of pending callbacks.
 *
 * @param a_fake True when it's not a real idle moment.
*/
void ev::loop::beanstalkd::Looper::Idle (const bool a_fake)
{
    // ... lock, perform and 'dequeue' pending callbacks ...
    std::lock_guard<std::mutex>(idle_callbacks_.mutex_);
    
    try {
        // ... nothing to do?
        if ( 0 == idle_callbacks_.queue_.size() ) {
            // ... ⚠️ since we're only expecting to cancel previously registered callbacks ...
            // ... and we're under a mutex, we can safely forget all cancellations ...
            idle_callbacks_.cancelled_.clear();
            // ... we're done ...
            return;
        }
        // ... process as many as we can ...
        const auto   start         = std::chrono::steady_clock::now();
        const size_t exec_timeout = ( a_fake ? 100 : 50 );
        while ( idle_callbacks_.queue_.size() > 0 ) {
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
                callback->function_(callback->id_);
                // ... recurrent?
                if ( true == callback->recurrent_ ) {
                    // ... yes, reschedule it ...
                    callback->end_tp_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(callback->timeout_);
                } else {
                    // ... no, forget it ...
                    idle_callbacks_.queue_.pop();
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
        // ... write to permanent log ...
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::Exception& a_cc_exception) {
           EV_LOOP_BEANSTALK_LOG("queue",
                                 "ERROR %s",
                                 a_cc_exception.what()
           );
        }
    }
}
