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

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_factory
 * @param a_callbacks
 * @param a_default_tube
 */
ev::loop::beanstalkd::Looper::Looper (const ev::Loggable::Data& a_loggable_data,
                                      const ev::loop::beanstalkd::Job::Factory& a_factory, const ev::loop::beanstalkd::Job::MessagePumpCallbacks& a_callbacks,
                                      const std::string a_default_tube)
    : ::ev::loop::beanstalkd::Object(a_loggable_data),
       factory_(a_factory), callbacks_(a_callbacks), default_tube_(a_default_tube)
{
    logger_client_->Unset(ev::LoggerV2::Client::LoggableFlags::IPAddress | ev::LoggerV2::Client::LoggableFlags::OwnerPTR);
    beanstalk_ = nullptr;
    job_ptr_   = nullptr;
    EV_LOOP_BEANSTALK_IF_LOG_ENABLED({
        ev::LoggerV2::GetInstance().Register(logger_client_, { "queue" });
    });
    polling_.timeout_ = -1.0;
    polling_.set_     = false;
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
 * @param a_beanstakd_config
 * @param a_output_directory
 * @param a_logs_directory
 * @param a_shared_directory
 * @param a_aborted
 */
void ev::loop::beanstalkd::Looper::Run (const ::ev::beanstalk::Config& a_beanstakd_config,
                                        const std::string& a_output_directory, const std::string& a_logs_directory, const std::string& a_shared_directory,
                                        volatile bool& a_aborted)
{   
    Beanstalk::Job job;
    std::string    uri;
    bool           cancelled;
    bool           success;
    bool           already_ran;
    bool           deferred;
    uint16_t       http_status_code;
    
    // ... write to permanent log ...
    EV_LOOP_BEANSTALK_LOG("queue",
                          "Connecting to %s:%d...",
                          a_beanstakd_config.host_.c_str(),
                          a_beanstakd_config.port_
    );

    beanstalk_ = new ::ev::beanstalk::Consumer(a_beanstakd_config);
    
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
    
    // ... write to permanent log ...
    EV_LOOP_BEANSTALK_LOG("queue",
                          "%s",
                          "Connection established..."
    );

    std::stringstream ss;
    for ( auto it : a_beanstakd_config.tubes_ ) {
        ss << it << ',';
    }
    ss.seekp(-1, std::ios_base::end); ss << '\0';
    EV_LOOP_BEANSTALK_LOG("queue",
                          "Listening to '%s' %s...",
                          ss.str().c_str(),
                          a_beanstakd_config.tubes_.size() != 1 ? "tubes" : "tube"
    );
    
    // TODO 2.0 : log service id
    //    EV_LOOP_BEANSTALK_LOG("queue",
    //                          "Using service id '%s'",
    //                          a_beanstakd_config.
    //    );

    // ... write to permanent log ...
    EV_LOOP_BEANSTALK_LOG("queue",
                          "WTNG %19.19s"  "-",
                          "--"
    );
    
    //
    // set polling timeout
    //
    uint32_t polling_timeout;
    if ( true == polling_.set_ ) {
        polling_timeout = 0;
    } else {
        polling_timeout = static_cast<uint32_t>(a_beanstakd_config.abort_polling_);
    }

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
        
        job_payload_ = Json::Value::null;
        if ( false == json_reader_.parse(job.body(), job_payload_) ) {
            delete beanstalk_; beanstalk_ = nullptr;
            throw ev::Exception("An error occurred while loading job payload: JSON parsing error - %s\n", json_reader_.getFormattedErrorMessages().c_str());
        }
        
        const std::string tube = job_payload_.get("tube", default_tube_).asString();

        // check if consumer is already loaded
        const auto cached_it = cache_.find(tube);
        if ( cache_.end() == cached_it ) {
            
            job_ptr_ = factory_(tube);
            
           // ... git current job an oportunity to write a message as if it was the consumer ...
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
            
            cache_[tube] = job_ptr_;
            job_ptr_->Setup(&callbacks_, a_output_directory, a_logs_directory, a_shared_directory);
        } else {
            job_ptr_ = cached_it->second;
        }
        
        // ... process job ...
        osal::ConditionVariable job_cv;
        
        const std::chrono::steady_clock::time_point start_tp = std::chrono::steady_clock::now();
        
        try {
            
            job_ptr_->Consume(/* a_id */ job.id(), /* a_body */ job_payload_,
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
            
            job_cv.Wait();

        } catch (...) {
            
            delete job_ptr_;
            cache_.erase(cache_.find(tube));

            job_cv.Wake();

        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_tp).count();
        
        job_ptr_ = nullptr;

        loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");

        // ... check print result ...
        if ( true == success || true == cancelled || true == already_ran || true == deferred || 404 == http_status_code ) {
            
            // ... write to permanent log?
            if ( false == deferred ) {
                // ... write it ...
                if ( 404 == http_status_code ) {
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA " ~> %s...",
                                          job.id(), "NOT FOUND"
                    );
                } else {
                    EV_LOOP_BEANSTALK_LOG("queue",
                                          "Job #" INT64_FMT_MAX_RA " ~> %s...",
                                          job.id(), ( true == already_ran ? "Ignored" : true == cancelled ? "Cancelled" : "Done" )
                    );
                }
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
        if ( false == deferred ) {
            EV_LOOP_BEANSTALK_LOG("queue",
                                  "Job #" INT64_FMT_MAX_RA " ~> FINISHED: " UINT64_FMT "ms.",
                                  job.id(),
                                  static_cast<uint64_t>(elapsed)
            );
        }
        
        // ... write to permanent log ...
        EV_LOOP_BEANSTALK_LOG("queue",
                              "WTNG %15.15s ---:",
                              " "
        );
        
        // ... since a call to asString() can allocate dynamic memory ...
        // ... set this object to null to free memory now ...
        job_payload_ = Json::Value::null;
        
        // ... next job ...
        Idle(/* a_fake */ true);
    }
    
    loggable_data_.Update(loggable_data_.module(), loggable_data_.ip_addr(), "consumer");
    
    // ... write to permanent log ...
    EV_LOOP_BEANSTALK_LOG("queue",
                          "%s",
                          "Stopped..."
    );
    
    //
    // release allocated memory, so this function can be called again
    // ( if required, eg: restart by signal )
    //
    if ( nullptr != beanstalk_ ) {
        delete beanstalk_;
        beanstalk_ = nullptr;
    }
    
    for ( auto it : cache_ ) {
        delete it.second;
    }

    cache_.clear();
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
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
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
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
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
