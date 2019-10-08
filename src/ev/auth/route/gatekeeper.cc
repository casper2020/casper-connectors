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

#include "osal/thread_helper.h"

#include "cc/utc_time.h"

#include <fstream> // std::filebuf

/*
 * STATIC DATA INITIALIZATION
 */

ev::auth::route::Gatekeeper::LoggerSettings ev::auth::route::Gatekeeper::s_logger_settings_ = {
    /* data_               */     nullptr,
    /* client_             */     nullptr,
    /* index_padding_      */           0,
    /* index_fmt_          */          "",
    /* method_fmt_         */ "[ %6.6s ]", // GET, POST, PATCH, DELETE
    /* section_            */          "",
    /* separator_          */          "",
    /* log_access_granted_ */       false
};

std::string ev::auth::route::Gatekeeper::s_config_uri_  = "";
bool        ev::auth::route::Gatekeeper::s_initialized_ = false;

/**
 * @brief Prepare singleton.
 *
 * @param a_loggable_data_ref Loggable data refererence, won't be ( copied a pointer to it will be set ).
 * @param a_uri               Gatekeeper JSON configuration file URI.
 */
void ev::auth::route::Gatekeeper::Startup (const Loggable::Data &a_loggable_data_ref,
                                           const std::string& a_uri)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    if ( true == s_initialized_ ) {
        throw ::ev::Exception("Gatekeeper already initialized!");
    }
        
    if ( nullptr == tmp_url_ ) {
        tmp_url_ = curl_url();
    }
    
    // ... reset reusable variables ...
    status_ = {
        /* code_ */      500,
        /* data_ */      Json::Value::null,
        /* deflected_ */ false
    };

    if ( nullptr == s_logger_settings_.data_ ) {
        s_logger_settings_.data_ = new ::ev::Loggable::Data(a_loggable_data_ref);
    }
    if ( nullptr == s_logger_settings_.client_ ) {
        s_logger_settings_.client_ = new ::ev::LoggerV2::Client(*s_logger_settings_.data_);
        ::ev::LoggerV2::GetInstance().Register(s_logger_settings_.client_, { "gatekeeper" });
    }
    
#ifdef __APPLE__
    s_logger_settings_.log_access_granted_ = true;
#endif
    
    // ... load configuration?
    if ( 0 != a_uri.length() ) {
        Load(a_uri, /* a_signo */ 0);
    }

    // ... we're done ...
    s_initialized_ = true;
}

/**
* @brief Reload gatekeeper configuration.
*
* @param a_signo Signal number that requested this action.
*/
void ev::auth::route::Gatekeeper::Reload (int a_signo)
{
    Load(s_config_uri_, a_signo);
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

    delete s_logger_settings_.client_;
    s_logger_settings_.client_ = nullptr;

    delete s_logger_settings_.data_;
    s_logger_settings_.data_ = nullptr;
    
    bribe_.bypass_methods_.clear();

    s_initialized_   = false;
}

#ifdef __APPLE__
#pragma mark - [PUBLIC] - API
#endif

/**
 * @brief Check if an URI is allowed.
 *
 * @param a_method        HTTP method name.
 * @param a_url           Configuration file local URI.
 * @param a_session       Current session data.
 * @param a_loggable_data Log data.
 *
 * @return One of HTTP status code.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::Allow (const std::string& a_method, const std::string& a_url, const ev::casper::Session& a_session,
                                                                               const Loggable::Data &a_loggable_data)
{
    return Allow(a_method, a_url, a_session, /* a_deflector */ nullptr, a_loggable_data);
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
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::Allow (const std::string& a_method, const std::string& a_url, const ev::casper::Session& a_session,
                                                                               ev::auth::route::Gatekeeper::Deflector a_deflector,
                                                                               const Loggable::Data &a_loggable_data)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
    
    try {

        // ... ensure this singleton is initialized ...
        if ( false == s_initialized_ ) {
            throw ::ev::Exception("Gatekeeper NOT initialized!");
        }

        // ... reset last status ...
        status_.code_      = 500;
        status_.data_      = Json::Value::null;
        status_.deflected_ = false;
                
        // .. if no rules loaded ...
        if ( 0 == rules_.size() ) {
            // ... passage allowed ...
            return SetAllowed(a_method, tmp_path_, /* a_rule */ nullptr);
        }

        // ... update loggable data ( s_logger_settings_.data_ is mutable and should be always updated  ) ...
        s_logger_settings_.data_->Update(a_loggable_data.module(), a_loggable_data.ip_addr(), a_loggable_data.tag());

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
            // ... was gatekeeper bribed?
            if ( bribe_.bypass_methods_.end() != bribe_.bypass_methods_.find(a_method) ) {
                // ... yes, allow passage because he is not looking ...
                return SetAllowed(a_method, tmp_path_, /* a_rule */ nullptr);
            } else {
                // ... passage denied - 404 - Not Found - no rule found for this path ...
                return SerializeError(a_method, tmp_path_, 404, /* a_rule */ nullptr, /* a_fields */ {});
            }
        }
        
        // ... if method is not supported ...
        if ( rule->methods_.end() == rule->methods_.find(a_method) ) {
            // ... serializer allowed methods to string ...
            auto ostream = std::ostringstream();
            std::copy(rule->methods_.begin(), rule->methods_.end(), std::ostream_iterator<std::string>(ostream, ","));
            const std::string allowed_methods = ostream.str();
            // ... set reason
            auto sstream = std::stringstream();
            sstream = std::stringstream();
            sstream << a_method << " not in ( " << allowed_methods << " )";
            const std::string reason = sstream.str();
            // ... 405 - Method Not Allowed
            return SerializeError(a_method, tmp_path_, 405, rule, /* a_fields */ {{ "method", a_method, allowed_methods, reason }});
        }
            
        // ... validate 'role_mask' ...
        const std::string   role_mask    = a_session.GetValue("role_mask", "");
        const unsigned long role_mask_ul = ( 0 != role_mask.length() ? std::stoul(role_mask, nullptr, 10) : 0 );
        if ( 0 != ( rule->role_mask_ & role_mask_ul ) ) {
            // ... passage granted, deflect job?
            if ( nullptr != rule->job_ ) {
                // ... yes ...
                if ( nullptr != a_deflector ) {
                    // ... call deflector and we're done ...
                    // ... we're done ...
                    return SetDeflected(a_method, tmp_path_, rule, a_deflector(rule->job_->tube_, rule->job_->ttr_, rule->job_->validity_));
                } else {
                    // ... 501 - Not Implemented - deflector not supported / implemented ...
                    return SerializeError(a_method, tmp_path_, 501, rule, /* a_fields */ {});
                }
            }
            // ... no, we'de done ...
            return SetAllowed(a_method, tmp_path_, rule);
        }

        // ... serialize expected and actual values of 'role mask' to hex string ...
        auto sstream = std::stringstream();
        sstream = std::stringstream();
        sstream << "0x" << std::setfill ('0') << std::setw(8) << std::hex << rule->role_mask_;
        const std::string expected_role_mask = sstream.str();
        sstream = std::stringstream();
        sstream << "0x" << std::setfill ('0') << std::setw(8) << std::hex << role_mask_ul;
        const std::string got_role_mask = sstream.str();
        sstream = std::stringstream();
        sstream << "0 == ( " << expected_role_mask << " & " << got_role_mask << " )";
        const std::string reason = sstream.str();
        // ... 401 - Access Denied - authenticated but invalid credentials ( in this case role_mask )
        return SerializeError(a_method, tmp_path_, 401, rule, /* a_fields */ {{ "role_mask", expected_role_mask, got_role_mask, reason }});
        
    } catch (const ::ev::Exception& a_ev_exception) {
        // ... 500 - Internal Server Error - oops ...
        return SerializeException(a_method, tmp_path_, a_ev_exception);
    }    
}

#ifdef __APPLE__
#pragma mark - [PRIVATE] - METHOD(S) / FUNCTION(S)
#endif

/**
 * @brief Load gatekeeper configuration.
 *
 * @param a_uri   Configuration file local URI.
 * @param a_signo Signal number that requested this action.
 */
void ev::auth::route::Gatekeeper::Load (const std::string& a_uri, const size_t a_signo)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();

    /*
     * {
     *   "rules":[
     *             {
     *                "methods": [""],
     *                "expr": "",
     *                "role_mask": "0x0",
     *                "job": {
     *                  "tube": "",
     *                  "methods": [""]
     *                 }
     *             }
     *     ],
     *   "bribes": {
     *     "methods": [""]
     *   },
     *   "options": {
     *     "logs" : {
     *        "log_access_granted" : true
     *      }
     *   }
     * }
     */
    s_logger_settings_.data_->Update(/* a_module */ "gatekeeper", /* a_ip_addr */ "", /* a_tag */ ( 0 == a_signo ? "startup" : "signal"));
    
    auto& logger = ::ev::LoggerV2::GetInstance();
    
    // ... log event ...
    const int logger_title_padding = std::max(static_cast<int>(a_uri.length()), 106);
        
    s_logger_settings_.section_   = "--- " + std::string(logger_title_padding, ' ') + " ---";
    s_logger_settings_.separator_ = "--- " + std::string(logger_title_padding, '-') + " ---";
    
    std::vector<std::string> log_lines;
    // ... present 'gatekeeper' ...
    if ( true == logger.IsRegistered(s_logger_settings_.client_, "gatekeeper") ) {
        log_lines.push_back(s_logger_settings_.separator_);
        log_lines.push_back(s_logger_settings_.section_);
        log_lines.push_back("--- " + cc::UTCTime::NowISO8601WithTZ());
        log_lines.push_back("--- " + a_uri);
        log_lines.push_back(s_logger_settings_.section_);
        logger.Log(s_logger_settings_.client_, "gatekeeper", log_lines);
    }
    
    try {
     
        std::filebuf file;
        if ( nullptr == file.open(a_uri, std::ios::in) ) {
            throw ::ev::Exception(("An error occurred while opening file '" + a_uri + "' to read gatekeeper configuration!" ));
        }
        std::istream in_stream(&file);
        
        Json::Reader reader;
        Json::Value  object;
        
        // ... parse configuration JSON file ...
        if ( false == reader.parse(in_stream, object) ) {
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
        const Json::Value array = object["rules"];
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
                        /* tube_     */ job["tube"].asString(),
                        /* ttr_      */ job.get("ttr"     , -1).asInt(),
                        /* validity_ */ job.get("validity", -1).asInt(),
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
            
            // ... load bribes ...
            const Json::Value bribes = object["bribes"];
            if  ( false == bribes.isNull() ) {
                const Json::Value& bribed_methods = bribes ["methods"];
                if ( false == bribed_methods.isNull() ) {
                    if ( false == bribed_methods.isArray() ) {
                        throw ::ev::Exception("An error ocurred while parsing gatekeeper bribe: an array of strings is expected!");
                    } else {
                        for ( Json::ArrayIndex idx = 0 ; idx < bribed_methods.size() ; ++idx ) {
                            if ( false == bribed_methods[idx].isString() ) {
                                throw ::ev::Exception("An error ocurred while parsing gatekeeper bribe: method at index " + std::to_string(static_cast<size_t>(idx)) + " is not a valid string!");
                            }
                            bribe_.bypass_methods_.insert(bribed_methods[idx].asString());
                        }
                    }
                }
            }
            
            // ... load options ...
            const Json::Value options = object["options"];
            if  ( false == options.isNull() ) {
                const Json::Value& logs = options["logs"];
                if ( false == logs.isNull() ) {
                    if ( false == logs.isObject() ) {
                        throw ::ev::Exception("An error ocurred while parsing gatekeeper options: an object is expected!");
                    } else {
                        s_logger_settings_.log_access_granted_ = logs.get("log_access_granted", s_logger_settings_.log_access_granted_).asBool();
                    }
                }
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
        s_config_uri_ = a_uri;

        // ... set log format rules ...
        s_logger_settings_.index_padding_ = ::ev::LoggerV2::NumberOfDigits(rules_.size()) + 1; // + 1 - sign
        s_logger_settings_.index_fmt_     = "[ %" + std::to_string(s_logger_settings_.index_padding_) + "zu ] %s";
        
        // ... log loaded rules && separators ...
        if ( true == logger.IsRegistered(s_logger_settings_.client_, "gatekeeper") ) {
            // ... rules ...
            for ( size_t idx = 0 ; idx < rules_.size() ; ++idx ) {
                Log(rules_[idx]);
            }
            // ... separators ...
            log_lines.clear();
            log_lines.push_back(s_logger_settings_.section_);
            log_lines.push_back(s_logger_settings_.separator_);
            // ...
            logger.Log(s_logger_settings_.client_, "gatekeeper", log_lines);
        }
        

    } catch (const Json::Exception& a_json_exception) {
        // ... translate exception and re-throw it ...
        throw ev::Exception("%s", a_json_exception.what());
    } catch (const ::ev::Exception& a_ev_exception) {
        // ... log event?
        if ( true == logger.IsRegistered(s_logger_settings_.client_, "gatekeeper") ) {
            log_lines.clear();
            log_lines.push_back(s_logger_settings_.section_);
            log_lines.push_back("Failed to load rules from '" + a_uri + "'");
            log_lines.push_back(a_ev_exception.what());
            log_lines.push_back(s_logger_settings_.section_);
            log_lines.push_back(s_logger_settings_.separator_);
            logger.Log(s_logger_settings_.client_, "gatekeeper", log_lines);
        }
        // ... rethrow exception ...
        throw a_ev_exception;
    }
}

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
#pragma mark - [PRIVATE] - Serialization Method(s) / Function(s)
#endif

/**
 * @brief Set 'allowed' data.
 *
 * @param a_method HTTP method name.
 * @param a_path   Request URL path component.
 * @param a_rule See \link Rule \link.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::SetAllowed (const std::string& a_method, const std::string& a_path,
                                                                                    const ev::auth::route::Gatekeeper::Rule *a_rule)
{
    // ... 200 - OK ...
    status_.code_      = 200;
    status_.deflected_ = false;
    // ... log data ...
    Log(a_method, a_path, /* a_status_code */ 200, a_rule, /* a_fields */ {}, /* a_data */ nullptr, /* a_exception */ nullptr);
    // ... we're done ...
    return status_;
}


/**
 * @brief Set 'allowed' and deflected data.
 *
 * @param a_method HTTP method name.
 * @param a_path   Request URL path component.
 * @param a_rule See \link Rule \link.
 * @param a_data See \link DeflectorData \link
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::SetDeflected (const std::string& a_method, const std::string& a_path,
                                                                                      const ev::auth::route::Gatekeeper::Rule *a_rule,
                                                                                      const ev::auth::route::Gatekeeper::DeflectorData& a_data)
{
    // ... 200 - OK ...
    status_.code_      = 200;
    status_.deflected_ = true;
    // ... log data ...
    Log(a_method, a_path, /* a_status_code */ 200, a_rule, /* a_fields */ {}, /* a_data */ &a_data, /* a_exception */ nullptr);
    // ... we're done ...
    return status_;
}

/**
 * @brief Serialize an error to a JSON object.
 *
 * @param a_method HTTP method name.
 * @param a_path   Request URL path component.
 * @param a_code   One of HTTP status code.
 * @param a_rule   See \link Rule \link.
 * @param a_fields Rule fields.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::SerializeError (const std::string& a_method, const std::string& a_path, const uint16_t a_code,
                                                                                        const ev::auth::route::Gatekeeper::Rule* a_rule, const Rule::Fields& a_fields)
{
    // ... an error ocurred, translate to a JSON api error object ...
    status_.code_           = a_code;
    status_.deflected_      = false;
    status_.data_           = Json::Value(Json::ValueType::objectValue);
    status_.data_["errors"] = Json::Value(Json::ValueType::arrayValue);
    
    Json::Value& object = status_.data_["errors"].append(Json::Value(Json::ValueType::objectValue));
    // ... set standard fields ...
    object["code"]   = "FORBIDDEN_BY_GATEKEEPER";
    object["detail"] = "";
    // ... set status message according to status code ...
    switch (a_code) {
        // 401 - Access Denied - authenticated but invalid credentials ( in this case role_mask )
        case 401:
        object["status"] = "401 - Access Denied";
        break;
        // 404 - Not Found - no rule found for this url
        case 404:
            object["status"] = "404 - Not Found";
            break;
        // 405 - Method Not Allowed
        case 405:
            object["status"] = "405 - Method Not Allowed";
            break;
        // 501 - Not Implemented
        case 501:
            // 501 - Not Implemented - when job deflection is enabled ( we don't support it here ! )
            object["status"] = "501 - Not Implemented";
            break;
        // 500 - Internal Server Error
        default:
            object["status"] = "500 - Internal Server Error";
            break;
    }
    // ... meta data for debug / logging proposes ...
    Json::Value& internal = object["meta"]["internal-error"];
    // ... fill 'meta/internal-error' data ...
    internal["method"] = a_method;
    internal["path"]   = a_path;
    if ( nullptr != a_rule ) {
        internal["why"] = "Access denied by rule at index " + std::to_string(a_rule->idx_) + ".";
    } else {
        internal["why"]  = "No rule found for this request.";
    }
    // ... log attempt ...
    Log(a_method, a_path, a_code, a_rule, a_fields,  /* a_data */ nullptr, /* a_exception */ nullptr);
    // ... we're done ...
    return status_;
}

/**
 * @brief Serialize an exception to a JSON object.
 *
 * @param a_method    HTTP method name.
 * @param a_fields    Rule fields.
 * @param a_session   Current session data.
 * @param a_exception Exception to serialize.
 */
const ev::auth::route::Gatekeeper::Status& ev::auth::route::Gatekeeper::SerializeException (const std::string& a_method, const std::string& a_path,
                                                                                            const ev::Exception& a_exception)
{
    // ... an error ocurred, translate to a JSON api error object ...
    status_.code_           = 500;
    status_.deflected_      = false;
    status_.data_           = Json::Value(Json::ValueType::objectValue);
    status_.data_["errors"] = Json::Value(Json::ValueType::arrayValue);
    
    Json::Value& object = status_.data_["errors"].append(Json::Value(Json::ValueType::objectValue));
    // ... set standard fields ...
    object["code"]   = "FORBIDDEN_BY_GATEKEEPER";
    object["detail"] = "";
    object["status"] = "500 - Internal Server Error";
    // ... meta data for debug / logging proposes ...
    Json::Value& internal = object["meta"]["internal-error"];
    // ... fill 'meta/internal-error' data ...
    internal["exception"] = a_exception.what();
    // ... log attempt ...
    Log(a_method, a_path, status_.code_, /* a_rule */ nullptr, /* a_fields */ {}, /* a_data */ nullptr, /* a_exception */ &a_exception);
    // ... we're done ...
    return status_;
}

#ifdef __APPLE__
#pragma mark - [PRIVATE] - Error(s) / Exception Serialization Method(s) / Function(s)
#endif

/**
 * @brief Log a \link Rule \link.
 *
 * @param a_rule See \link Rule \link.
 */
void ev::auth::route::Gatekeeper::Log (const ev::auth::route::Gatekeeper::Rule* a_rule) const
{
    // ... is logger token is enabled?
    auto& logger = ::ev::LoggerV2::GetInstance();
    if ( false == logger.IsRegistered(s_logger_settings_.client_, "gatekeeper") ) {
        // ... no ... we're done ...
        return;
    }
    
    // ... log rule ...
    auto ostream = std::ostringstream();
    std::copy(a_rule->methods_.begin(), a_rule->methods_.end(), std::ostream_iterator<std::string>(ostream, ","));

    logger.Log(s_logger_settings_.client_, "gatekeeper",
               s_logger_settings_.index_fmt_.c_str(),
               a_rule->idx_, a_rule->expr_.str_.c_str()
    );
        
    logger.Log(s_logger_settings_.client_, "gatekeeper",
               "%*c %18.18s: %s", static_cast<int>(s_logger_settings_.index_padding_ + 4),
               ' ', "Allowed Methods", ostream.str().c_str()
    );
    
    logger.Log(s_logger_settings_.client_, "gatekeeper",
               "%*c %18.18s: 0x%08lx", static_cast<int>(s_logger_settings_.index_padding_ + 4),
               ' ', "Required Role Mask", a_rule->role_mask_
    );
    
    // ... done?
    if ( nullptr == a_rule->job_ ) {
        return;
    }
    
    // ... log job ...
    ostream = std::ostringstream();
    std::copy(a_rule->job_->methods_.begin(), a_rule->job_->methods_.end(), std::ostream_iterator<std::string>(ostream, ","));
    logger.Log(s_logger_settings_.client_, "gatekeeper",
               "%*c %18.18s: deflected to tube '%s' when method is one of ( %s )",
               static_cast<int>(s_logger_settings_.index_padding_ + 4),
               ' ', "Job", a_rule->job_->tube_.c_str(), ostream.str().c_str()
    );
    logger.Log(s_logger_settings_.client_, "gatekeeper",
               "%*c %18.18s: %d",
               static_cast<int>(s_logger_settings_.index_padding_ + 4),
               ' ', "TTR", static_cast<int>(a_rule->job_->ttr_)
    );
    logger.Log(s_logger_settings_.client_, "gatekeeper",
               "%*c %18.18s: %d",
               static_cast<int>(s_logger_settings_.index_padding_ + 4),
               ' ', "Validity", static_cast<int>(a_rule->job_->validity_)
    );
}

/**
 * @brief Log an error / exception.
 *
 * @param a_method    HTTP method name.
 * @param a_path      Request URL path component.
 * @param a_code      One of HTTP status code.
 * @param a_rule      See \link Rule \link.
 * @param a_fields    Rule fields.
 * @param a_data      See \link DeflectorData \link
 * @param a_exception Exception to log.
 */
void ev::auth::route::Gatekeeper::Log (const std::string& a_method, const std::string& a_path, const uint16_t& a_status_code,
                                       const ev::auth::route::Gatekeeper::Rule* a_rule, const Rule::Fields& a_fields, const ev::auth::route::Gatekeeper::DeflectorData* a_data,
                                       const ev::Exception* a_exception) const
{    // ... is logger token is enabled?
    auto& logger = ::ev::LoggerV2::GetInstance();
    if ( false == logger.IsRegistered(s_logger_settings_.client_, "gatekeeper") ) {
        // ... no ... we're done ...
        return;
    }

    // ... if access granted but should not be logged ...
    if ( 200 == a_status_code && false == s_logger_settings_.log_access_granted_ ) {
        // ... we're done ...
        return;
    }

    //,    GET, 200,  2, ...
    //,    GET, 404, -1, ...
    auto sstream = std::stringstream();
    sstream << std::setfill(' ') << std::setw(6) << a_method << ", " << std::setfill(' ') << std::setw(3) << std::to_string(a_status_code);
    sstream.clear();
    
    // ... if access granted ...
    if ( 200 == a_status_code ) {
        // ... success because ...
        if ( nullptr != a_rule ) {
            // ... a rule allowed ...
            sstream << ", " << std::setfill(' ') << std::setw(static_cast<int>(s_logger_settings_.index_padding_)) << a_rule->idx_;
            sstream << ", allowed by rule";
            if ( nullptr != a_rule->job_ ) {
                sstream << " and deflected to '" << a_rule->job_->tube_ << "' ( ttr = " << a_data->ttr_ << ", validity = " << a_data->validity_ << " )";
            }
        } else if ( 0 == rules_.size() ) {
            // ... not rules loaded ...
            sstream << ", " << std::setfill(' ') << std::setw(static_cast<int>(s_logger_settings_.index_padding_)) << -1;
            sstream << ", there are no rules";
        } else {
            sstream << ", " << std::setfill(' ') << std::setw(static_cast<int>(s_logger_settings_.index_padding_)) << -1;
            sstream << ", gatekeeper was bribed";
        }
    } else if ( 401 == a_status_code || 405 == a_status_code ) { // ... access denied or method not allowed ...
        // ... because rule denied ...
        sstream << ", " << std::setfill(' ') << std::setw(static_cast<int>(s_logger_settings_.index_padding_)) << a_rule->idx_;
        if ( 401 == a_status_code ) {
            sstream << ", denied by rule";
        } else { /* 405 == a_status_code */
            sstream << ", method not allowed";
        }
        if ( a_fields.size() > 0 ) {
            sstream << " ( ";
            for ( size_t idx = 0 ; idx < a_fields.size() ; ++idx ) {
                if ( idx > 0 ) {
                    sstream << ", ";
                }
                sstream << a_fields[idx].reason_;
            }
            sstream << " )";
        }
    } else {
        sstream << ", " << std::setfill(' ') << std::setw(static_cast<int>(s_logger_settings_.index_padding_)) << -1;
        if ( 404 == a_status_code ) { // ... not found ...
            // ... because no rule aplies ...
            sstream << ", rule not found";
        } else if ( 501 == a_status_code ) {
            // ... because it's not implemented ...
            sstream << ", not implemented";
        } else {
            // ... because rule denied ...
            sstream << ", status code: " << a_status_code << " - " << a_exception->what();
        }
    }
    
    // ... log entry ...
    logger.Log(s_logger_settings_.client_, "gatekeeper", "%s", a_path.c_str());
    logger.Log(s_logger_settings_.client_, "gatekeeper", "%s", sstream.str().c_str());
}
