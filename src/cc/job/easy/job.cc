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

#include "cc/i18n/singleton.h"

/**
 * @brief Default constructor.
 *
 * @param a_tube
 * @param a_config
 * @param a_loggable_data

 */
cc::job::easy::Job::Job (const ev::Loggable::Data& a_loggable_data, const std::string& a_tube, const Config& a_config)
    : ev::loop::beanstalkd::Job(a_loggable_data, a_tube, a_config),
      CC_IF_DEBUG_CONSTRUCT_SET_VAR(thread_id_, cc::debug::Threading::GetInstance().CurrentThreadID(), ,)
      log_level_(a_config.other().get("log_level", CC_JOB_LOG_LEVEL_INF).asInt())
{
    logger_client_->Unset(ev::LoggerV2::Client::LoggableFlags::IPAddress | ev::LoggerV2::Client::LoggableFlags::OwnerPTR);
    CC_JOB_LOG_REGISTER();
}

/**
 * @brief Destructor
 */
cc::job::easy::Job::~Job ()
{
    CC_JOB_LOG_UNREGISTER();
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
    
    CC_DEBUG_LOG_MSG("job", "Job #" INT64_FMT " ~> request:\n%s",
                     ID(), a_payload.toStyledString().c_str()
    );
    
    try {
        // ... run job ...
        Run(a_id, a_payload, run_response);
        // ... was job cancelled?
        if ( true == WasCancelled() && false == Deferred() ) { // deferred jobs just must handle with cancellation themselves ...
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
        } else {
            // ... set final response ...
            if ( 200 == run_response.code_ ) {
                (void)SetCompletedResponse(run_response.payload_, job_response);
            } else if ( 302 == run_response.code_ ) {
                run_response.code_ = SetRedirectResponse(run_response.payload_, job_response, 200);
            } else {
                if ( false == run_response.payload_.isNull() ) {
                    (void)SetFailedResponse(run_response.code_, run_response.payload_, job_response);
                } else {
                    (void)SetFailedResponse(run_response.code_, job_response);
                }
            }
        }
    } catch (const ::cc::Exception& a_cc_exception) {
        // ...unhandled exception - known type ...
        run_response.code_ = SetInternalServerError(/* a_i18n */ NULL, /* a_exception */ {
            /* code_      */ "CC Exception",
            /* exception_ */ a_cc_exception
        }, run_response.payload_);
        (void)SetFailedResponse(run_response.code_, run_response.payload_, job_response);
     } catch (...) {
         // ...unhandled exception / error - unknown type ...
         try {
             ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
         } catch (const ::cc::Exception& a_cc_exception) {
             run_response.code_ = SetInternalServerError(/* a_i18n */ NULL, /* a_exception */ {
                 /* code_      */ "Generic Exception",
                 /* exception_ */ a_cc_exception
             }, run_response.payload_);
             (void)SetFailedResponse(run_response.code_, run_response.payload_, job_response);
         }
     }
    
    // ... check if we should finalize now ...
    if ( false == AlreadyRan() && true == Deferred() ) {
        // ... log ...
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("STATUS", "%s",
                                        "DEFERRED"
        );
        // ... notify deferred ...
        a_deferred_callback();
        // ... we're done, for now ...
        return;
    }
    
    // ... log ...
    EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("RESPONSE", "%s", json_writer_.write(job_response).c_str());
                        
    // ... publish result ...
    Finished(job_response ,
            [this, &job_response]() {
                // ... log ...
                CC_DEBUG_LOG_MSG("job", "Job #" INT64_FMT " ~> response:\n%s",
                                 ID(), json_styled_writer_.write(job_response).c_str()
                );
            },
            [this](const ev::Exception& a_ev_exception){
                // ... log ...
                EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("EXCEPTION", "%s", a_ev_exception.what());
                // ... for debug proposes only ...
                CC_DEBUG_LOG_TRACE("job", "Job #" INT64_FMT " ~> exception: %s",
                                   ID(), a_ev_exception.what()
                );
                (void)(a_ev_exception);
            }
    );
    
    //                     //
    //   Final Callbacks   //
    //          &          //
    //     'Logging'       //
    //                     //
    if ( true == WasCancelled() || true == AlreadyRan() ) {
        // ... log ...
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("STATUS", "%s", WasCancelled() ? "CANCELLED" : "ALREADY RAN");
        // ... notify ...
        a_cancelled_callback(AlreadyRan());
    } else {
        // ... log ...
        EV_LOOP_BEANSTALK_JOB_LOG_QUEUE("STATUS", "%s",
                                        job_response["status"].asCString()
        );
        // ... notify ...
        a_completed_callback("", ( 200 == run_response.code_ ), run_response.code_);
    }
}

/**
 * @brief Create a 'message' object.
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetI18NMessage (const uint16_t& a_code, const easy::Job::I18N& a_i18n, Json::Value& o_payload)
{
    // ... a_i18n is mandatory!
    if ( nullptr == a_i18n.key_ ) {
        throw ::cc::Exception("a_i18n can't be nullptr!");
    }
    // ... create 'message' object ...
    o_payload            = Json::Value(Json::ValueType::objectValue);
    o_payload["message"] = Json::Value(Json::ValueType::arrayValue);
    o_payload["message"].append(a_i18n.key_);
    for ( auto arg : a_i18n.arguments_ ) {
        o_payload["message"].append(Json::Value(Json::ValueType::objectValue))[arg.first] = arg.second;
    }
    // ... done ...
    return a_code;
}

/**
 * @brief Create an 'meta' object.
 *
 * @param a_code    HTTP status code.
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetI18NError (const uint16_t& a_code, const easy::Job::I18N& a_i18n, const easy::Job::InternalError& a_error,
                                           Json::Value& o_payload)
{
    // ... set i18n ...
    (void)SetI18NMessage(a_code, a_i18n, o_payload);
    // ... append 'meta' object - for internal usage only ...
    o_payload["meta"]["internal-error"] = Json::Value(Json::ValueType::objectValue);
    if ( nullptr != a_error.code_ ) {
        o_payload["meta"]["internal-error"]["code"] = a_error.code_;
    } else {
        const auto it = cc::i18n::Singleton::k_http_status_codes_map_.find(a_code);
        if ( cc::i18n::Singleton::k_http_status_codes_map_.end() != it ) {
            o_payload["meta"]["internal-error"]["code"] = it->second;
        } else {
            o_payload["meta"]["internal-error"]["code"] = "???";
        }
    }
    try {
        ParseJSON(a_error.why_, o_payload["meta"]["internal-error"]["why"]);
    } catch (...) {
        o_payload["meta"]["internal-error"]["why"] = a_error.why_;
    }
    // ... done ...
    return a_code;
}

/**
 * @brief Fill a 'ok' payload ( 200 - Ok ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetOk (const easy::Job::I18N* a_i18n, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        return SetI18NMessage(200, *a_i18n, o_payload);
    }
    return SetI18NMessage(200, /* a_i18n */ { /* a_key*/ "i18n_completed", /* a_args */ {} }, o_payload);
}

/**
 * @brief Fill a 'bad request' payload ( 400 - Bad Request ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetBadRequest (const easy::Job::I18N* a_i18n, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        return SetI18NMessage(400, *a_i18n, o_payload);
    }
    return SetI18NMessage(400, /* a_i18n */ { /* a_key*/ "i18n_bad_request", /* a_args */ {} }, o_payload);
}

/**
 * @brief Fill a 'internal server error' payload ( 500 - Internal Server Error ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetInternalServerError (const easy::Job::I18N* a_i18n, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        return SetI18NMessage(400, *a_i18n, o_payload);
    }
    return SetI18NMessage(400, /* a_i18n */ { /* a_key*/ "i18n_internal_server_error", /* a_args */ {} }, o_payload);
}

/**
 * @brief Fill a 'not found' payload ( 404 - Not Found ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetNotFound (const easy::Job::I18N* a_i18n, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        return SetI18NMessage(404, *a_i18n, o_payload);
    }
    return SetI18NMessage(404, /* a_i18n */ { /* a_key*/ "i18n_not_found", /* a_args */ {} }, o_payload);
}

/**
 * @brief Fill a 'not found' payload ( 404 - Not Found ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetNotFound (const I18N* a_i18n, const easy::Job::InternalError& a_error, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        (void)SetI18NMessage(404, *a_i18n, o_payload);
    } else {
        (void) SetI18NMessage(404, /* a_i18n */ { /* a_key*/ "i18n_not_found", /* a_args */ {} }, o_payload);
    }
    o_payload["meta"]["internal-error"] = Json::Value(Json::ValueType::objectValue);
    if ( nullptr != a_error.code_ ) {
        o_payload["meta"]["internal-error"]["code"] = a_error.code_;
    } else {
        o_payload["meta"]["internal-error"]["code"] = "400 - Not Found";
    }
    o_payload["meta"]["internal-error"]["why"]   = a_error.why_;
    return 404;
}

/**
 * @brief Fill a 'timeout' payload ( 408 - Request Timeout ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetTimeout (const easy::Job::I18N* a_i18n, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        return SetI18NMessage(408, *a_i18n, o_payload);
    }
    return SetI18NMessage(408, /* a_i18n */ { /* a_key*/ "i18n_timeout", /* a_args */ {} }, o_payload);
}

/**
 * @brief Fill a 'internal server error' payload ( 500 - Internal Server Error ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 * @param a_error   Internal error.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetInternalServerError (const easy::Job::I18N* a_i18n, const easy::Job::InternalError& a_error, Json::Value& o_payload)
{
    (void)SetInternalServerError(a_i18n, o_payload);
    o_payload["meta"]["internal-error"] = Json::Value(Json::ValueType::objectValue);
    if ( nullptr != a_error.code_ ) {
        o_payload["meta"]["internal-error"]["code"] = a_error.code_;
    } else {
        o_payload["meta"]["internal-error"]["code"] = "500 - Internal Server Error";
    }
    o_payload["meta"]["internal-error"]["why"]   = a_error.why_;
    return 500;
}

/**
 * @brief Fill a 'internal server error' payload ( 500 - Internal Server Error ).
 *
 * @param a_i18n      I18 message.
 * @param o_payload   Payload object to create.
 * @param a_exception Internal exception.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetInternalServerError (const easy::Job::I18N* a_i18n, const easy::Job::InternalException& a_exception, Json::Value& o_payload)
{
    return SetInternalServerError(a_i18n, /* a_error */ InternalError({ /* code_ */ a_exception.code_, /* why_ */ a_exception.excpt_.what() }), o_payload);
}

/**
 * @brief Fill a 'not implemented' payload ( 501 - Not Implemented ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
*
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetNotImplemented (const easy::Job::I18N* a_i18n, Json::Value& o_payload)
{
    if ( nullptr != a_i18n ) {
        return SetI18NMessage(501, *a_i18n, o_payload);
    }
    return SetI18NMessage(501, /* a_i18n */ { /* a_key*/ "i18n_not_implemented", /* a_args */ {} }, o_payload);
}

/**
 * @brief Fill a 'not implemented' payload ( 501 - Not Implemented ).
 *
 * @param a_i18n    I18 message.
 * @param o_payload Payload object to create.
 * @param a_error   Internal error.
 *
 * @return HTTP status code.
 */
uint16_t cc::job::easy::Job::SetNotImplemented (const easy::Job::I18N* a_i18n, const easy::Job::InternalError& a_error, Json::Value& o_payload)
{
    (void)SetNotImplemented(a_i18n, o_payload);
    o_payload["meta"]["internal-error"] = Json::Value(Json::ValueType::objectValue);
    if ( nullptr != a_error.code_ ) {
        o_payload["meta"]["internal-error"]["code"] = a_error.code_;
    } else {
        o_payload["meta"]["internal-error"]["code"] = "501 - Not Implemented";
    }
    o_payload["meta"]["internal-error"]["why"]   = a_error.why_;
    return 501;
}
