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
 * @param a_factories
 */
ev::loop::beanstalkd::Looper::Looper (const ev::loop::beanstalkd::Looper::Factories& a_factories)
    : factories_(a_factories)
{
    consumer_     = nullptr;
    consumer_ptr_ = nullptr;
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Looper::~Looper ()
{
    if ( nullptr != consumer_ ) {
        delete consumer_;
    }
    for ( auto it : consumers_ ) {
        delete it.second;
    }
    consumer_ptr_ = nullptr;
}

/**
 * @brief Consumer loop.
 *
 * @param a_loggable_data
 * @param a_beanstakd_config
 */
void ev::loop::beanstalkd::Looper::Run (ev::Loggable::Data& a_loggable_data,
                                        const ::ev::beanstalk::Config& a_beanstakd_config)
{   
    bool aborted = false;
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
    
    consumer_ = new ::ev::beanstalk::Consumer(a_beanstakd_config);
    
    a_loggable_data.SetTag("consumer");
    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                  "Connection established..."
    );
    
    // ... write to permanent log ...
    ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                  "Waiting..."
    );
    
    //
    // consumer loop
    //
    while ( consumer_->Reserve(job) && false == aborted ) {
        
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
            throw ev::Exception("An error occurred while loading configuration: JSON parsing error - %s\n", json_reader_.getFormattedErrorMessages().c_str());
        }
        
        const std::string tube = job_payload_["tube"].asString();

        // check if consumer is already loaded
        const auto cached_consumer_it = consumers_.find(tube);
        if ( consumers_.end() == cached_consumer_it ) {
            const auto consumer_factory_it = factories_.find(tube);
            if ( factories_.end() == consumer_factory_it ) {
                throw ev::Exception("Job tube '%s' is not registered!", tube.c_str());
            }
            consumer_ptr_    = consumer_factory_it->second();
            consumers_[tube] = consumer_ptr_;
        } else {
            consumer_ptr_ = cached_consumer_it->second;
        }
        
        // ... do the actual 'print' a.k.a. render PDF ( and make a .ZIP if required )  ...
        osal::ConditionVariable job_cv;
        
        // TODO wrap with try catch ...
        
        consumer_ptr_->Consume(/* a_id */
                               job.id(),
                               /* a_body */
                               job_payload_,
                               /* a_callback */
                               [&uri, &http_status_code, &success, &job_cv](const std::string& a_uri, const bool a_success, const uint16_t a_http_status_code) {
                                   uri              = a_uri;
                                   http_status_code = a_http_status_code;
                                   success          = a_success;
                                   job_cv.Wake();
                               },
                               /* a_cancelled_callback */
                               [&cancelled,&job_cv] () {
                                   cancelled = true;
                                   job_cv.Wake();
                               }
        );
        job_cv.Wait();
        
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
            consumer_->Del(job);
        } else {
            
            // ... write to permanent log ...
            ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                          "Job #" INT64_FMT_MAX_RA " ~> Buried: making it available for human inspection",
                                          job.id()
            );
            
            // ... bury it - making it available for human inspection ....
            consumer_->Bury(job);
        }
        
        a_loggable_data.SetTag("consumer");
        
        // ... write to permanent log ...
        ev::Logger::GetInstance().Log("queue", a_loggable_data,
                                      "Waiting..."
        );
        
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
    if ( nullptr != consumer_ ) {
        delete consumer_;
        consumer_ = nullptr;
    }
    
    for ( auto it : consumers_ ) {
        delete it.second;
    }
    consumers_.clear();
    
}
