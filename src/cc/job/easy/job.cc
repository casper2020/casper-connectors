/**
* @file job.h
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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

#include "cc/job/easy/job.h"

#include "cc/macros.h"

#include "cc/debug/types.h"

/**
 * @brief Default constructor.
 *
 * @param a_tube
 * @param a_config
 * @param a_loggable_data

 */
cc::job::easy::Job::Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config)
    : ev::loop::beanstalkd::Job(a_loggable_data, a_tube, a_config)
{
    /* empty */
}

/**
 * @brief Destructor
 */
cc::job::easy::Job::~Job ()
{
    /* empty */
}

/**
 * @brief Run a 'payroll' calculation.
 *
 * @param a_id                  Job ID
 * @param a_payload             Job Payload
 * @param a_completed_callback
 * @param a_cancelled_callback
 * @param a_deferred_callback
 */
void cc::job::easy::Job::Run (const int64_t& a_id, const Json::Value& a_payload,
                              const cc::job::easy::Job::CompletedCallback& a_completed_callback,
                              const cc::job::easy::Job::CancelledCallback& a_cancelled_callback,
                              const cc::job::easy::Job::DeferredCallback& a_deferred_callback)
{
    Json::Value                  job_response  = Json::Value::null;
    cc::job::easy::Job::Response run_response { 400, Json::Value::null };
    
    std::string uri_;
    
    CC_DEBUG_LOG_TRACE("job", "Job #" INT64_FMT " ~> request:\n%s", ID(), a_payload.toStyledString().c_str());
    
    try {

        Run(a_id, a_payload, run_response);
        
        if ( 200 == run_response.code_ ) {
            SetCompletedResponse(run_response.payload_, job_response);
        } else {
            SetFailedResponse(run_response.code_, job_response);
        }
        
    } catch (const ::cc::Exception& a_cc_exception) {
        AppendError(/* a_type */ "CC Exception", /* a_why */ a_cc_exception.what(), /* a_where */__FUNCTION__, /* a_code */ -1);
     } catch (...) {
         try {
             ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
         } catch (const ::cc::Exception& a_cc_exception) {
             AppendError(/* a_type */ "Generic Exception", /* a_why */ a_cc_exception.what(), /* a_where */__FUNCTION__, /* a_code */ -1);
         }
     }
    
    // ... was job cancelled?
    if ( true == WasCancelled() ) {
        // ... yes, publish notification ...
        Publish({
            /* key_    */ nullptr,
            /* args_   */ {},
            /* status_ */ cc::job::easy::Job::Status::Cancelled,
            /* value_  */ -1.0,
            /* now_    */ true
        });
        // ... set 'cancelled' response ...
        SetCancelledResponse(/* a_payload*/ Json::Value::null, /* o_response */ job_response );
    } else if ( false == HasErrorsSet() && true == HasFollowUpJobs() ) { // ... has follow up job?
        // ... submit follow up jobs ...
        if ( false == PushFollowUpJobs() ) {
            SetFailedResponse(/* a_code */ run_response.code_, /* o_response */ job_response );
        }
    } else if ( true == HasErrorsSet() ) { // ... failed?
        // ... override any response, with a failure message with collected errors ( if any )  ...
        SetFailedResponse(/* a_code */ run_response.code_, /* o_response */ job_response );
    }
    
    // ... check if we should finalize now ...
    if ( not ( true == HasErrorsSet() ||  true == WasCancelled() || true == AlreadyRan() ) && true == Deferred() ) {
        // ... log ...
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("STATUS", "%s",
                                        "DEFERRED"
        );
        // ... notify deferred ...
        a_deferred_callback();
        // ... we're done, for now ...
        return;
    }
    
    const std::string response_str = json_styled_writer_.write(job_response);
    
    // ... for logging proposes ...
    if ( true == HasErrorsSet() ) {
        // ... log data ...
        EV_LOOP_BEANSTALK_JOB_LOG("error",
                                  "payload:%s", json_writer_.write(a_payload).c_str()
        );
        EV_LOOP_BEANSTALK_JOB_LOG("error",
                                  "response:%s", response_str.c_str()
        );
    }
                        
    // ... publish result ...
    Finished(job_response ,
            [this, &job_response , &response_str]() {
                CC_DEBUG_LOG_TRACE("job", "Job #" INT64_FMT " ~> response:\n%s", ID(), response_str.c_str());
            },
            [this](const ev::Exception& a_ev_exception){
                CC_DEBUG_LOG_TRACE("job", "Job #" INT64_FMT " ~> exception: %s", ID(), a_ev_exception.what());
                (void)(a_ev_exception);
            }
    );
    
    //                     //
    //   Final Callbacks   //
    //          &          //
    //     'Logging'       //
    //                     //
    if ( true == WasCancelled() || true == AlreadyRan() ) {
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("STATUS", "%s", WasCancelled() ? "CANCELLED" : "ALREADY RAN");
        a_cancelled_callback(AlreadyRan());
    } else {
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("STATUS", "%s",
                                        job_response ["status"].asCString()
        );
        a_completed_callback("", false == HasErrorsSet(), run_response.code_);
    }
}
