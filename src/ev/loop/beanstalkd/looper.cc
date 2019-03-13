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

#include "ev/logger.h"
#include "ev/exception.h"

#include "osal/osalite.h"
#include "osal/condition_variable.h"

#include "json/json.h"

/**
 * @brief Default constructor.
 *
 * @param _factory
 * @param a_callbacks
 * @param a_default_tube
 */
ev::loop::beanstalkd::Looper::Looper (const ev::loop::beanstalkd::Job::Factory& a_factory, const ev::loop::beanstalkd::Job::MessagePumpCallbacks& a_callbacks, const std::string a_default_tube)
    : factory_(a_factory), callbacks_(a_callbacks),
      default_tube_(a_default_tube)
{
    beanstalk_ = nullptr;
    job_ptr_   = nullptr;
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
}

/**
 * @brief Consumer loop.
 *
 * @param a_loggable_data
 * @param a_beanstakd_config
* @param a_aborted
 */
void ev::loop::beanstalkd::Looper::Run (ev::Loggable::Data& a_loggable_data,
                                        const ::ev::beanstalk::Config& a_beanstakd_config,
                                        volatile bool& a_aborted)
{   
    Beanstalk::Job job;
    std::string    uri;
    bool           cancelled;
    bool           success;
    uint16_t       http_status_code;
    
    // ... write to permanent log ...
    std::stringstream ss;
    for ( auto it : a_beanstakd_config.tubes_ ) {
        ss << it << ',';
    }
    ss.seekp(-1, std::ios_base::end); ss << '\0';
    ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                  "Trying to connect to %s:%d using '%s' tubes...",
                                  a_beanstakd_config.host_.c_str(),
                                  a_beanstakd_config.port_,
                                  ss.str().c_str()
    );
    
    beanstalk_ = new ::ev::beanstalk::Consumer(a_beanstakd_config);
    
    a_loggable_data.SetTag("consumer");
    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                  "Connection established..."
    );
    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                  "Waiting..."
    );
    
    const ev::loop::beanstalkd::Job::MessagePumpCallbacks message_pump_callbacks = {
        /* on_fatal_exception_      */ callbacks_.on_fatal_exception_,
        /* dispatch_on_main_thread_ */ callbacks_.dispatch_on_main_thread_
    };
    
    //
    // consumer loop
    //
    while ( false == a_aborted ) {
        
        // ... test abort flag, every n seconds ...
        if ( false == beanstalk_->Reserve(job, a_beanstakd_config.abort_polling_) ) {
            continue;
        }
        
        a_loggable_data.SetTag("consumer");
      
        // ... write to permanent log ...
        ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                      "Job #" INT64_FMT_MAX_RA " ~> Processing...",
                                      job.id()
        );
        
        uri          = "";
        cancelled    = false;
        success      = false;
        
        job_payload_ = Json::Value::null;
        if ( false == json_reader_.parse(job.body(), job_payload_) ) {
            throw ev::Exception("An error occurred while loading job payload: JSON parsing error - %s\n", json_reader_.getFormattedErrorMessages().c_str());
        }
        
        const std::string tube = job_payload_.get("tube", default_tube_).asString();

        // check if consumer is already loaded
        const auto cached_it = cache_.find(tube);
        if ( cache_.end() == cached_it ) {
            job_ptr_ = factory_(tube);
            cache_[tube] = job_ptr_;
            job_ptr_->Setup(&message_pump_callbacks);
        } else {
            job_ptr_ = cached_it->second;
        }
        
        // ... process job ...
        osal::ConditionVariable job_cv;
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
                              [&cancelled,&job_cv] () {
                                  cancelled = true;
                                  job_cv.Wake();
                              }
            );
            
            job_cv.Wait();

        } catch (...) {
            
            delete job_ptr_;
            cache_.erase(cache_.find(tube));

            job_cv.Wait();

        }
        
        job_ptr_ = nullptr;
        
        // ... check print result ...
        if ( true == success || true == cancelled || 404 == http_status_code ) {
            
            // ... write to permanent log ...
            if ( 404 == http_status_code ) {
                ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                              "Job #" INT64_FMT_MAX_RA " ~> %s...",
                                              job.id(), "NOT FOUND"
                );
            } else {
                ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                              "Job #" INT64_FMT_MAX_RA " ~> %s...",
                                              job.id(), ( true == cancelled ? "Cancelled" : "Done" )
                );
            }
            
            // ... we're done with it ...
            beanstalk_->Del(job);
        } else {
            
            // ... write to permanent log ...
            ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                          "Job #" INT64_FMT_MAX_RA " ~> Buried: making it available for human inspection",
                                          job.id()
            );
            
            // ... bury it - making it available for human inspection ....
            beanstalk_->Bury(job);
        }
        
        a_loggable_data.SetTag("consumer");
        
        // ... write to permanent log ...
        ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                      "Waiting..."
        );
        
        // ... since a call to asString() can allocate dynamic memory ...
        // ... set this object to null to free memory now ...
        job_payload_ = Json::Value::null;
        
    }
    
    a_loggable_data.SetTag("consumer");
    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("queue", a_loggable_data,
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
