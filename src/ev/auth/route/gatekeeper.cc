/**
 * @file gatekeeper.cc
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
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

#include "ev/auth/route/gatekeeper.h"

#include "ev/exception.h"

#include "cc/utc_time.h"

#include <fstream> // std::filebuf

/*
 * STATIC DATA INITIALIZATION
 */
::ev::Loggable::Data*   ev::auth::route::Gatekeeper::s_loggable_data_        = nullptr;
::ev::LoggerV2::Client* ev::auth::route::Gatekeeper::s_logger_client_        = nullptr;
size_t                  ev::auth::route::Gatekeeper::s_logger_index_padding_ = 0;
std::string             ev::auth::route::Gatekeeper::s_logger_index_fmt_     = "";
std::string             ev::auth::route::Gatekeeper::s_logger_method_fmt_    = "[ %6.6s ]"; // GET, POST, PATCH, DELETE
std::string             ev::auth::route::Gatekeeper::s_logger_section_       = "";
std::string             ev::auth::route::Gatekeeper::s_logger_separator_     = "";
bool                    ev::auth::route::Gatekeeper::s_initialized_          = false;

/**
 * @brief Prepare singleton.
 *
 * @param a_loggable_data_ref Loggable data refererence, won't be ( copied a pointer to it will be set ).
 * @param a_uri               Gatekeeper JSON configuration file URI.
 */
void ev::auth::route::Gatekeeper::Startup (const Loggable::Data &a_loggable_data_ref, const std::string& a_uri)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    if ( true == s_initialized_ ) {
        throw ::ev::Exception("Gatekeeper already initialized!");
    }
    
    tmp_url_          = curl_url();
    tmp_status_       = {
        /* code_ */ 500,
        /* data_ */ Json::Value::null
    };

    s_loggable_data_  = new ::ev::Loggable::Data(a_loggable_data_ref);
    s_logger_client_  = new ::ev::LoggerV2::Client(*s_loggable_data_);
    s_initialized_    = true;
        
    // ... load configuration ...
    if ( 0 != a_uri.length() ) {
        Load(a_uri, /* a_signo */ 0);
    }
}

/**
 * @brief Release all previously allocated data.
 */
void ev::auth::route::Gatekeeper::Shutdown ()
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    if ( false == s_initialized_ ) {
        return;
    }
    
    if ( NULL != tmp_url_ ) {
        curl_url_cleanup(tmp_url_);
        tmp_url_ = nullptr;
    }
    for ( auto rule : rules_ ) {
        delete rule;
    }

    delete s_logger_client_;
    s_logger_client_ = nullptr;

    delete s_loggable_data_;
    s_loggable_data_ = nullptr;
    
    s_initialized_   = false;
}

/**
 * @brief Load gatekeeper configuration.
 *
 * @param a_uri   Configuration file local URI.
 * @param a_signo Sinal number that requested this action.
 */
void ev::auth::route::Gatekeeper::Load (const std::string& a_uri, const size_t a_signo)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    if ( false == s_initialized_ ) {
        throw ::ev::Exception("Gatekeeper NOT initialized!");
    }

    /*
     * [
     *   {
     *      "methods": [""],
     *      "expr": "",
     *      "role_mask": "0x0",
     *      "job": {
     *        "tube": "",
     *        "methods": [""]
     *       }
     *   }
     * ]
     */
    ::ev::LoggerV2::GetInstance().Unregister(s_logger_client_);
    s_loggable_data_->SetTag(( 0 == a_signo ? "startup" : "signal"));
    ::ev::LoggerV2::GetInstance().Register(s_logger_client_, { "gatekeeper" });

    // ... log event ...
    const int logger_title_padding = std::max(static_cast<int>(a_uri.length()), 80);
    
    s_logger_section_   = "--- " + std::string(logger_title_padding, ' ') + " ---";
    s_logger_separator_ = "--- " + std::string(logger_title_padding, '-') + " ---";
    
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "%s", s_logger_separator_.c_str()
     );
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "--- %s", cc::UTCTime::NowISO8601WithTZ().c_str()
     );
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "--- %s", a_uri.c_str()
     );
    
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "%s", s_logger_section_.c_str()
     );
    try {
     
        std::filebuf file;
        if ( nullptr == file.open(a_uri, std::ios::in) ) {
            throw ::ev::Exception(("An error occurred while opening file '" + a_uri + "' to read gatekeeper configuration!" ));
        }
        std::istream in_stream(&file);
        
        Json::Reader reader;
        Json::Value  array;
        
        // ... parse configuration JSON file ...
        if ( false == reader.parse(in_stream, array) ) {
            // ... an error occurred ...
            const auto errors = reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw ::ev::Exception("An error ocurred while parsing gatekeeper configuration: %s!",
                                      reader.getFormatedErrorMessages().c_str()
                );
            } else {
                throw ::ev::Exception("An error ocurred while parsing gatekeeper configuration!");
            }
        }
        
        // ... make sure it's a valid array ...
        if ( false == array.isArray() ) {
            throw ::ev::Exception("An error ocurred while parsing gatekeeper configuration: an array of objects is expected!");
        }
        
        std::vector<Rule*> new_rules;
        
        try {
            //
            const auto json_string_array_to_set = [] (const Json::ArrayIndex a_idx, const Json::Value& a_array, std::set<std::string>& o_set) {
                for ( Json::ArrayIndex idx = 0 ; idx < a_array.size() ; ++idx ) {
                    const Json::Value& value = a_array[idx];
                    if ( false == value.isString() || 0 == value.asString().length() ) {
                        throw ::ev::Exception("An error ocurred while parsing gatekeeper configuration: element at %d is not a valid object!", a_idx);
                    }
                    o_set.insert(value.asString());
                }
            };
            std::set<std::string> rule_methods;
            std::set<std::string> job_methods;
            Gatekeeper::Rule::Job* job_obj = nullptr;
            // ... ensure it's a valid array of objects and load regexp ...
            for ( Json::ArrayIndex idx = 0 ; idx < array.size() ; ++idx ) {
                // ... pick rule definitions ...
                const Json::Value& object    = array[idx];
                const Json::Value& methods   = object["methods"];
                const Json::Value& role_mask = object["role_mask"];
                const Json::Value& expr      = object["expr"];
                const Json::Value& job       = object["job"];
                // ... ensure we've a valid object ...
                if ( false == object.isObject() || false == expr.isString() || false == methods.isArray() || false == role_mask.isString() ) {
                    // ... notify error ...
                    throw ::ev::Exception("An error ocurred while parsing gatekeeper configuration: element at %d is not a valid object!", idx);
                }
                json_string_array_to_set(idx, methods, rule_methods);
                // ... job ?
                if ( false == job.isNull()  ) {
                    // ... validate job object ...
                    if ( false == job.isObject() || false == job["tube"].isString() || false == job["methods"].isArray() ) {
                        // ... notify error ...
                        throw ::ev::Exception("An error ocurred while parsing gatekeeper configuration: element at %d / job is not a valid object!", idx);
                    }
                    // ... grab job ...
                    json_string_array_to_set(idx, job["methods"], job_methods);
                    job_obj = new Gatekeeper::Rule::Job({
                        /* tube_    */ job["tube"].asString(),
                        /* methods_ */ job_methods
                    });
                }
                // ... keep track of new rule ...
                new_rules.push_back(new Gatekeeper::Rule(
                    /* a_idx       */ static_cast<size_t>(idx),
                    /* a_expr      */ { expr.asString(), std::regex(expr.asString(), std::regex_constants::ECMAScript | std::regex_constants::icase) },
                    /* a_methods   */ rule_methods,
                    /* a_role_mask */ std::stoul(role_mask.asString(), nullptr, 16),
                    /* a_job       */ job_obj
                ));
                // ... clean up ...
                job_obj = nullptr;
                rule_methods.clear();
                job_methods.clear();
            }
            
        } catch (const ::ev::Exception& a_ev_exception) {
            // ... release collected 'new' rules ...
            for ( auto it : new_rules ) {
                delete it;
            }
            // ... rethrow ...
            throw a_ev_exception;
        }

        // ... clear old rules ...
        for ( auto rule : rules_ ) {
            delete rule;
        }
        rules_.clear();
        
        // ... keep track of new ones ...
        for ( auto rule : new_rules ) {
            rules_.push_back(rule);
        }

        // ... set log format rules ...
        s_logger_index_padding_ = ::ev::LoggerV2::NumberOfDigits(rules_.size());
        s_logger_index_fmt_     = "[ %" + std::to_string(s_logger_index_padding_) + "zu ] %s";
        
        // ... log loaded rules ...
        for ( size_t idx = 0 ; idx < rules_.size() ; ++idx ) {
            Log(rules_[idx]);
        }
        // ... log separators ...
        ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                          "%s", s_logger_section_.c_str()
         );
        ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                          "%s", s_logger_separator_.c_str()
         );
    } catch (const ::ev::Exception& a_ev_exception) {
        // ... log event ...
        ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                          "Failed to load rules from '%s': %s",
                                          a_uri.c_str(), a_ev_exception.what()
         );
        // ... log separators ...
        ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                          "%s", s_logger_section_.c_str()
         );
        ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                          "%s", s_logger_separator_.c_str()
         );
        // ... rethrow exception ...
        throw a_ev_exception;
    }
}

/**
 * @brief Check if an URI is allowed.
 *
 * @param a_method  HTTP method name.
 * @param a_url     Configuration file local URI.
 * @param a_session Current session data.
 *
 * @return One of HTTP status code.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::Allow (const char* const a_method, const std::string& a_url, const ev::casper::Session& a_session,
                                                                               ev::auth::route::Gatekeeper::JobDeflector a_deflector)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    try {

        // ... ensure this singleton is initialized ...
        if ( false == s_initialized_ ) {
            throw ::ev::Exception("Gatekeeper NOT initialized!");
        }

        // ... reset last status ...
        tmp_status_.code_ = 200;
        tmp_status_.data_ = Json::Value::null;

        if ( 0 == rules_.size() ) {
            return tmp_status_;
        }
        
        // ... start as 'internal server error'  ...
        tmp_status_.code_ = 500;

        // ... TODO
        
        s_loggable_data_->Update("TODO", "TODO");

        // ... extract path and check if it match any rule ...
        ExtractURLComponent(a_url, CURLUPart::CURLUPART_PATH, tmp_path_);
        
        // ... check if path match any available rule
        const Gatekeeper::Rule* rule = nullptr;
        for ( auto r : rules_ ) {
            if ( true == std::regex_match(tmp_path_, tmp_match_, r->expr_.regex_) ) {
                rule = r;
                break;
            }
        }
        
        // ... if no rule match ...
        if ( nullptr == rule ) {
            // ... passage denied - 404 - Not Found - no rule found for this path ...
            return SerializeError(a_method, tmp_path_, a_session, rule, 404);
        }
            
        // ... validate 'role_mask' ...
        const std::string role_mask = a_session.GetValue("role_mask", "");
        if ( 0 != role_mask.length() && ( rule->role_mask_ & std::stoul(role_mask, nullptr, 16) ) ) {
            // ... passage granted, deflect job?
            if ( nullptr != rule->job_ ) {
                // ... yes ...
                if ( nullptr != a_deflector ) {
                    a_deflector(rule->job_->tube_);
                } else {
                    // ... 501 - Not Implemented - deflector not supported / implemented ...
                    return SerializeError(a_method, tmp_path_, a_session, rule, 501);
                }
            }
            // ... no, we'de done ...
            tmp_status_.code_ = 200;
            return tmp_status_;
        }

        // ... 401 - Access Denied - authenticated but invalid credentials ( in this case role_mask )
        return SerializeError(a_method, tmp_path_, a_session, rule, 401);
        
    } catch (const ::ev::Exception& a_ev_exception) {
        // ... 500 - Internal Server Error - oops ...
        return SerializeException(a_method, tmp_path_, a_session, a_ev_exception);
    }    

    return tmp_status_;
}

#ifdef __APPLE__
#pragma mark - [PRIVATE] - METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Extract a URL component value ( string representation ).
 *
 * @param a_url   Configuration file local URI.
 * @param a_part  One of \link CURLUPart \link.
 * @param o_value Part value, string representation.
 *
 * @return Component value, string representation.
*/
void ev::auth::route::Gatekeeper::ExtractURLComponent (const std::string& a_url, const CURLUPart a_part, std::string& o_value)
{
    const CURLUcode set_rc = curl_url_set(tmp_url_, CURLUPART_URL, a_url.c_str(), /* flags */ 0);
    if ( CURLUE_OK != set_rc ) {
        throw ::ev::Exception("Unable to set a CURLU value!");
    }

    char*           part_value  = NULL;
    const CURLUcode part_get_rc = curl_url_get(tmp_url_, a_part, &part_value, /* flags */ 0);
    if ( CURLUE_OK == part_get_rc ) {
        o_value = part_value;
        curl_free(part_value);
    } else {
        if ( NULL != part_value ) {
            curl_free(part_value);
        }
        throw ::ev::Exception("Unable to extract a CURLU part value!");
    }
 }

#ifdef __APPLE__
#pragma mark - [PRIVATE] - Error(s) / Exception Serialization Method(s) / Function(s)
#endif

/**
 * @brief Serialize an error to a JSON object.
 *
 * @param a_method  HTTP method name.
 * @param a_path    Configuration file local URI.
 * @param a_session Current session data.
 * @param a_rule    See \link Rule \link.
 * @param a_code    One of HTTP status code.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::SerializeError (const char* const a_method, const std::string& a_path, const ev::casper::Session& a_session,
                                                                                        const ev::auth::route::Gatekeeper::Rule* a_rule, const uint16_t a_code)
{
    // ... an error ocurred, translate to a JSON api error object ...
    tmp_status_.code_           = a_code;
    tmp_status_.data_           = Json::Value(Json::ValueType::objectValue);
    tmp_status_.data_["errors"] = Json::Value(Json::ValueType::arrayValue);
    
    Json::Value& object = tmp_status_.data_["errors"].append(Json::Value(Json::ValueType::objectValue));
    // ... set standard fields ...
    object["code"]   = "FORBIDDEN_BY_GATEKEEPER";
    object["detail"] = "";
    // ... set status message according to status code ...
    switch (a_code) {
        case 401:
        // 401 - Access Denied - authenticated but invalid credentials ( in this case role_mask )
        object["status"] = "403 - Access Denied";
        break;
        // 404 - Not Found - no rule found for this url
        case 404:
            object["status"] = "404 - Not Found";
            break;
        case 501:
            // 501 - Not Implemented - when job deflection is enabled ( we don't support it here ! )
            object["status"] = "501 - Not Implemented";
            break;
        default:
            object["status"] = "500 - Internal Server Error";
            break;
    }
    // ... meta data for debug / logging proposes ...
    Json::Value& internal = object["meta"]["internal-error"];
    // ... fill 'meta/internal-error' data ...
    if ( nullptr != a_rule ) {
        internal["why"] = "Access denied by rule #" + std::to_string(a_rule->idx_ + 1) + ".";
    } else {
        internal["why"]  = "No rule found for this request.";
    }
    // ... log attempt ...
    Log(a_method, a_code, a_rule, /* a_exception */ nullptr);
    // ... we're done ...
    return tmp_status_;
}

/**
 * @brief Serialize an exception to a JSON object.
 *
 * @param a_method    HTTP method name.
 * @param a_path      Configuration file local URI.
 * @param a_session   Current session data.
 * @param a_exception Exception to serialize.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::SerializeException (const char* const a_method, const std::string& a_path, const ev::casper::Session& a_session,
                                                                                            const ev::Exception& a_exception)
{
    // ... serialize common error data ...
    (void)SerializeError(a_method, a_path, a_session, /* a_rule */ nullptr, /* a_code */ 500);
    // ... serialize internal error 'exception' message ...
    tmp_status_.data_["errors"][0]["meta"]["internal-error"]["exception"] = a_exception.what();
    // ... we're done ...
    return tmp_status_;
}

#ifdef __APPLE__
#pragma mark - [PRIVATE] - Error(s) / Exception Serialization Method(s) / Function(s)
#endif

/**
 * @brief Log a \link Rule \link.
 *
 * @param a_rule See \link Rule \link.
 */
void ev::auth::route::Gatekeeper::Log (const ev::auth::route::Gatekeeper::Rule* a_rule)
{
    tmp_ostream_.str("");
    std::copy(a_rule->methods_.begin(), a_rule->methods_.end(), std::ostream_iterator<std::string>(tmp_ostream_, ","));

    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      s_logger_index_fmt_.c_str(),
                                      a_rule->idx_, a_rule->expr_.str_.c_str()
    );
        
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "%*c %18.18s: %s", static_cast<int>(s_logger_index_padding_ + 4),
                                      ' ', "Allowed Methods", tmp_ostream_.str().c_str()
    );
    
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "%*c %18.18s: 0x%08lx", static_cast<int>(s_logger_index_padding_ + 4),
                                      ' ', "Required Role Mask", a_rule->role_mask_
    );
    
    // ... done?
    if ( nullptr == a_rule->job_ ) {
        return;
    }
    
    // ... log job ...
    
    tmp_ostream_.str("");
    std::copy(a_rule->job_->methods_.begin(), a_rule->job_->methods_.end(), std::ostream_iterator<std::string>(tmp_ostream_, ","));
    
    ::ev::LoggerV2::GetInstance().Log(s_logger_client_, "gatekeeper",
                                      "%*c %18.18s: deflected to tube '%s' when method is one of ( %s )", static_cast<int>(s_logger_index_padding_ + 4),
                                      ' ', "Job", a_rule->job_->tube_.c_str(), tmp_ostream_.str().c_str()
    );
}

/**
 * @brief Log an error / exception.
 *
 * @param a_method    HTTP method name.
 * @param a_code      One of HTTP status code.
 * @param a_rule      See \link Rule \link.
 * @param a_exception Exception to log.
 */
void ev::auth::route::Gatekeeper::Log (const char* const a_method, const uint16_t& a_status_code, const ev::auth::route::Gatekeeper::Rule* a_rule,
                                       const ev::Exception* a_exception)
{
    // ... if access granted ...
    if ( 200 == a_status_code ) {
        // ... nothing to log ...
        return;
    }
    auto& logger = ::ev::LoggerV2::GetInstance();
    // ... access denied ...
    if ( 401 == a_status_code ) {
        // ... because rule denied ...
        const std::string fmt = "[ %3u ] - %6.6s - denied by rule %s [ %" + std::to_string(s_logger_index_padding_) + "zu ] %s";
        logger.Log(s_logger_client_, "gatekeeper",
                                      fmt.c_str(),
                                      a_status_code, a_method, a_rule->idx_, a_rule->expr_.str_.c_str()
        );
    } else if ( 404 == a_status_code ) {
        // ... because no rule aplies ...
        logger.Log(s_logger_client_, "gatekeeper",
                                      "[ %3u ] - %6.6s - rule not found",
                                      a_status_code, a_method
        );
    } else if ( 501 == a_status_code ) {
        // ... because rule denied ...
        logger.Log(s_logger_client_, "gatekeeper",
                                      "[ %3u ] - %6.6s - denied by status code %u - Not Implemented",
                                      a_status_code, a_method, a_status_code
        );
    } else {
        // ... because rule denied ...
        logger.Log(s_logger_client_, "gatekeeper",
                   "[ %3u ] - %6.6s - denied by status code %u -%s",
                   a_status_code, a_method, a_status_code, a_exception->what()
        );
    }
}
