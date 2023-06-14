/**
 * @file processor.cc
 *
 * Copyright (c) 2017-2023 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/modsecurity/processor.h"

#include "cc/exception.h"
#include "cc/fs/dir.h"
#include "cc/fs/file.h"

#include <signal.h> // SIGTTIN

// MARK: - ProcessorOneShotInitializer

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::modsecurity::ProcessorOneShotInitializer::ProcessorOneShotInitializer (cc::modsecurity::Processor& a_instance)
    : ::cc::Initializer<cc::modsecurity::Processor>(a_instance)
{
    instance_.loggable_data_  = nullptr;
    instance_.logger_client_  = nullptr;
}

/**
 * @brief Destructor.
 */
cc::modsecurity::ProcessorOneShotInitializer::~ProcessorOneShotInitializer ()
{
    instance_.CleanUp();
}

// MARK: - Processor

const std::regex cc::modsecurity::Processor::sk_details_id_regex_   = std::regex(".*\\[id\\s\"([^\\\"]+)\"\\].*");
const std::regex cc::modsecurity::Processor::sk_details_msg_regex_  = std::regex(".*\\[msg\\s\"([^\\\"]+)\"\\].*");
const std::regex cc::modsecurity::Processor::sk_details_file_regex_ = std::regex(".*\\[file\\s\"([^\\\"]+)\"\\].*");
const std::regex cc::modsecurity::Processor::sk_details_line_regex_ = std::regex(".*\\[line\\s\"([^\\\"]+)\"\\].*");
const std::regex cc::modsecurity::Processor::sk_details_data_regex_ = std::regex(".*\\[data\\s\"([^\\\"]+)\"\\].*");

/**
 * @brief Default constructor.
 */
cc::modsecurity::Processor::Processor ()
{
    /* empty */
}

/**
 * @brief Destructor
 */
cc::modsecurity::Processor::~Processor ()
{
    /* empty */
}

// MARK: -

/**
 * @brief Initializer processor, one-shot call only!
 *
 * @param a_data Loggable data to copy.
 */
void cc::modsecurity::Processor::Startup (const ::ev::Loggable::Data& a_data
                                          CC_IF_DEBUG(,const std::string& a_log_dir))
{
    // ... one-shot call only ...
    if ( nullptr != loggable_data_ || nullptr != logger_client_ ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__);
    }
    // ... new instances ...
    loggable_data_ = new ::ev::Loggable::Data(a_data);
    logger_client_ = new ::ev::LoggerV2::Client(*loggable_data_);
    log_dir_       = a_log_dir;
    // ... register token ...
    ::ev::LoggerV2::GetInstance().Register(logger_client_, { "cc-modsecurity" });
}

/**
 * @brief Enable a specific instance for a module.
 *
 * @param a_module   Module ID.
 * @param a_path     Config directory path.
 * @param a_file     Main config file.
 */
void cc::modsecurity::Processor::Enable (const std::string& a_module,
                                         const std::string& a_path, const std::string& a_file)
{
    const std::string uri = cc::fs::Dir::Normalize(a_path) + a_file;
    if ( false == cc::fs::File::Exists(uri) ) {
        // ... config does not exist ...
        throw ::cc::Exception("Configuration file '%s' for '%s' not found!", uri.c_str(), a_module.c_str());
    }
    // ... already present?
    {
        const auto it = instances_.find(a_module);
        if ( instances_.end() != it ) {
            delete it->second->rules_set_;
            delete it->second->mod_security_;
            CC_IF_DEBUG(delete it->second->debug_log_);
            delete it->second;
            instances_.erase(it);
        }
    }
    // ... new instance ...
    instances_[a_module] = Processor::NewInstance(a_module, uri, /* a_first */ true);
}

/**
 * @brief Shutdown processor, one-shot call only!
 */
void cc::modsecurity::Processor::Shutdown ()
{
    CleanUp();
}

/**
 * @brief Recycle processor by reloading rules.
 */
void cc::modsecurity::Processor::Recycle ()
{
    std::map<std::string, std::string> instances;
    // ... map module -> config and release current instances ...
    for ( const auto& it : instances_ ) {
        instances[it.first] = it.second->config_uri_;
        delete it.second->rules_set_;
        delete it.second->mod_security_;
        CC_IF_DEBUG(delete it.second->debug_log_);
        delete it.second;
    }
    instances_.clear();
    // ... create new instances ...
    for ( const auto& it : instances ) {
        instances_[it.first] = Processor::NewInstance(it.first, it.second, /* a_first */ false);
    }
}

// MARK: -

/**
 * @brief Simulate an HTTP request and validate it's content.
 *
 * @param a_module  Module ID.
 * @param a_request Request data.
 * @param o_rule    If denied, object will be filled with rule data.
 * @param o_lines   If set, log line will be copied to this vector.
 */
#define CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(...) \
if ( true == RequiredIntervention([&] { __VA_ARGS__;  return transaction; }, it, o_rule) ) { \
    delete transaction; \
    const auto elapsed = static_cast<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - scan_start).count()); \
    if ( 0 != o_rule.id_.length() ) { \
        _logger("   rule: " + o_rule.file_ + ":" + std::to_string(o_rule.line_)); \
        _logger("   " + o_rule.data_); \
        _logger("DENIED by rule # " +  o_rule.id_ + ": " + o_rule.msg_ + " ( took " + std::to_string(elapsed) + "ms to scan data )"); \
    } else { \
         _logger("DENIED due to " + o_rule.data_ + " ( took " + std::to_string(elapsed) + "ms to scan data )"); \
    } \
    return o_rule.code_; \
}
int cc::modsecurity::Processor::Validate (const Processor::HTTPPOSTRequest& a_request, Processor::Rule& o_rule)
{
    // ... reset ...
    o_rule = { /* id_ */ "" , /* msg */ "", /* file */ "", /* line_ */ 0, /* data_ */ "", /* code_ */ 200 };
    
    // ... grab instance ...
    Processor::Instance* instance;
    {
        const auto it = instances_.find(a_request.module_);
        if ( instances_.end() == it ) {
            throw ::cc::Exception("modsecurity not - %s!", "initialized");
        }
        instance = it->second;
    }
    
    // ... prepare log ...
    auto _logger = [&] (const std::string& a_line) {
        ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modsecurity", "%s", a_line.c_str());
        if ( nullptr != a_request.logger_ ) {
            a_request.logger_(a_line);
        }
    };
    // ... update loggable data ( loggable_data_ is mutable and should be always updated  ) ...
    loggable_data_->Update(loggable_data_->module(), a_request.client_.ip_, a_request.module_);
    
    // ... log ...
    _logger("scanning...");
    _logger("   " + a_request.content_type_ + ", " + std::to_string(a_request.content_length_) + " byte(s)");
    _logger("   file: " + a_request.body_file_uri_);
    
    // ... process ...
    ::modsecurity::Transaction* transaction = new ::modsecurity::Transaction(instance->mod_security_, instance->rules_set_, /* logCbData */ this);
    try {
        ::modsecurity::ModSecurityIntervention it;
        ::modsecurity::intervention::reset(&it);
        // ... keep track of tp ...
        const auto scan_start = std::chrono::steady_clock::now();
        // ... set connection info ...
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
            transaction->processConnection(a_request.client_.ip_.c_str(), a_request.client_.port_,
                                           a_request.server_.ip_.c_str(), a_request.server_.port_
            );
        );
        // ... set uri ...
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
            transaction->processURI(a_request.uri_.c_str(), "POST", a_request.version_.c_str());
        );
        // ... headers ...
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
            if ( 1 != transaction->addRequestHeader("Content-Type", a_request.content_type_) ) {
                throw ::cc::Exception("modsecurity unable to set %s!", "request 'Content-Type' header");
            }
        );
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
            if ( 1 != transaction->processRequestHeaders() ) {
                throw ::cc::Exception("modsecurity unable to process %s!", "request headers");
            }
        );
        // ... body ...
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
           if ( 1 != transaction->requestBodyFromFile(a_request.body_file_uri_.c_str()) ) {
               throw ::cc::Exception("modsecurity unable to set %s!", "request body");
           }
        );
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
            if ( 1 != transaction->processRequestBody() ) {
                throw ::cc::Exception("modsecurity unable to process %s!", "request body");
            }
        );
        const auto elapsed  = static_cast<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - scan_start).count());
        // ... prepare log lines ...
        _logger("PASS, no threats found ( took " + std::to_string(elapsed) + "ms to scan data )");
        // ... clean up ...
        delete transaction;
    } catch (const cc::Exception& a_cc_exception) {
        // ... clean up ...
        delete transaction;
        // ... log ...
        _logger(a_cc_exception.what());
        // ... and notify ...
        throw a_cc_exception;
    } catch (...) {
        // ... clean up ...
        delete transaction;
        // ... log and notify ...
        try {
            CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
        } catch (const cc::Exception& a_cc_exception) {
            // ... log ...
            _logger(a_cc_exception.what());
            // ... and notify ...
            throw a_cc_exception;
        }
    }
    // ... done ...
    return o_rule.code_;
}

// MARK: -

/**
 * @brief Initialize processor.
 *
 * @param a_module Module ID.
 * @param a_uri    Configuration file local URI.
 * @param a_first  True if it is the first instance created.
 */
cc::modsecurity::Processor::Instance*
cc::modsecurity::Processor::NewInstance (const std::string& a_module,
                                         const std::string& a_uri, const bool a_first) const
{
    const size_t logger_padding = std::max<size_t>(a_uri.length(), 106);
    // ...
    Processor::Instance* instance;
    try {
        // ... debug ...
    CC_IF_DEBUG(
        std::string error;
        ::modsecurity::DebugLog* debug_log = new ::modsecurity::DebugLog();
        debug_log->setDebugLogLevel(9);
        debug_log->setDebugLogFile(cc::fs::Dir::Normalize(log_dir_) + "cc-modsecurity-debug.log", &error);
    );
        // ... new instance ...
        instance = new Processor::Instance {
            /* mod_security_ */ new ::modsecurity::ModSecurity(),
            /* rules_set_    */ new ::modsecurity::RulesSet(CC_IF_DEBUG(debug_log)),
            /* debug_log_    */ CC_IF_DEBUG(debug_log),
            /* config_uri_   */ a_uri,
            /* log_config_   */ {
                /* padding_   */ logger_padding,
                /* section_   */ "--- " + std::string(logger_padding, ' ') + " ---",
                /* separator_ */ "--- " + std::string(logger_padding, '-') + " ---"
            }
        };
        // ... setup modsecurity instance ...
        instance->mod_security_->setConnectorInformation("casper-connectors");
        // ... load new rules ...
        if ( instance->rules_set_->loadFromUri(instance->config_uri_.c_str()) < 0 ) {
            throw ::cc::Exception("%s", instance->rules_set_->getParserError().c_str());
        }
        // ... log ...
        Log(a_module, *instance, a_first);
    } catch (...) {
        // ... clean up ...
        delete instance->mod_security_;
        delete instance->rules_set_;
        CC_IF_DEBUG(delete instance->debug_log_);
        delete instance;
        // ... notify ...
        CC_EXCEPTION_RETHROW(/* a_unhandled */ false);
    }
    // ... done ...
    return instance;
}

/**
 * @brief Release previously allocated memory.
 */
void cc::modsecurity::Processor::CleanUp ()
{
    for ( const auto& it : instances_ ) {
        delete it.second->rules_set_;
        delete it.second->mod_security_;
        CC_IF_DEBUG(delete it.second->debug_log_);
        delete it.second;
    }
    instances_.clear();
    if ( nullptr != loggable_data_ ) {
        delete loggable_data_;
        loggable_data_ = nullptr;
    }
    if ( nullptr != logger_client_ ) {
        ::ev::LoggerV2::GetInstance().Unregister(logger_client_);
        delete logger_client_;
        logger_client_ = nullptr;
    }
}

/**
 * @brief Load rules.
 *
 * @param a_module   Module ID.
 * @param a_instance Instance to log.
 * @param a_first    True if it is the first instance created.
 */
void cc::modsecurity::Processor::Log (const std::string& a_module,
                                      const Processor::Instance& a_instance, const bool a_first) const
{
    // ... update loggable data ( loggable_data_ is mutable and should be always updated  ) ...
    loggable_data_->Update(/* a_module */ "cc-modsecurity", /* a_ip_addr */ "", /* a_tag */ "~ " + std::string( a_first ? "load" : "reload") + " ~ " + a_module );
        
    auto& logger = ::ev::LoggerV2::GetInstance();
    
    // ... present ...
    std::vector<std::string> log_lines;
    log_lines.push_back(a_instance.log_config_.separator_);
    log_lines.push_back(a_instance.log_config_.section_);
    log_lines.push_back("--- " + cc::UTCTime::NowISO8601WithTZ());
    log_lines.push_back("--- " + a_instance.config_uri_);
    log_lines.push_back(a_instance.log_config_.section_);
    logger.Log(logger_client_, "cc-modsecurity", log_lines);

    // ... log minimalist rules data ...
    size_t number_of_rules = 0;
    for ( int phase_no = 0 ; phase_no < ::modsecurity::Phases::NUMBER_OF_PHASES; phase_no++ ) {
        const auto rs = a_instance.rules_set_->m_rulesSetPhases.at(phase_no);
        // ...
        const char* phase_cstr;
        switch (static_cast<::modsecurity::Phases>(phase_no)) {
            case ::modsecurity::Phases::ConnectionPhase:
                phase_cstr = "connection";
                break;
            case ::modsecurity::Phases::UriPhase:
                phase_cstr = "URI";
                break;
            case ::modsecurity::Phases::RequestHeadersPhase:
                phase_cstr = "request headers";
                break;
            case ::modsecurity::Phases::RequestBodyPhase:
                phase_cstr = "request body";
                break;
            case ::modsecurity::Phases::ResponseHeadersPhase:
                phase_cstr = "response headers";
                break;
            case ::modsecurity::Phases::ResponseBodyPhase:
                phase_cstr = "response body";
                break;
            case ::modsecurity::Phases::LoggingPhase:
                phase_cstr = "logging";
                break;
            default:
                phase_cstr = "???";
                break;
        }
        // ... phase info ...
        logger.Log(logger_client_, "cc-modsecurity",
                   "[ %2d ] %s rule(s)",
                   phase_no, phase_cstr
        );
        // ... rules ...
        for ( int rule_no = 0; rule_no < static_cast<int>(rs->size()); rule_no++ ) {
            logger.Log(logger_client_, "cc-modsecurity",
                       "\t%s", rs->at(rule_no)->getReference().c_str()
            );
        }
        number_of_rules += rs->size();
    }
    // ... finalize presentation ...
    logger.Log(logger_client_, "cc-modsecurity", "%s", a_instance.log_config_.section_.c_str());
    logger.Log(logger_client_, "cc-modsecurity", SIZET_FMT " rule(s) loaded", number_of_rules);
    logger.Log(logger_client_, "cc-modsecurity", "%s", a_instance.log_config_.separator_.c_str());
}

// MARK: -

/**
 * @brief Run a block of code and then check if it is necessary an intervention.
 *
 * @param a_transation   Transaction to check.
 * @param o_intervention Intervention data.
 * @param o_rule         If an intervention is required, fill this object with available data extracted from intervertion rule.
 */
bool cc::modsecurity::Processor::RequiredIntervention (const std::function<::modsecurity::Transaction*()>& a_callback,
                                                       ::modsecurity::ModSecurityIntervention& o_intervention, Rule& o_rule)
{
    ::modsecurity::Transaction* transaction = a_callback();
    // ... intervention is required?
    if ( false == transaction->intervention(&o_intervention) ) {
        // ... no ...
        return false;
    }
    // ... yes ...
    if ( nullptr != o_intervention.log ) {
        const std::string data = std::string(o_intervention.log);
        std::smatch match;
        typedef struct {
            const std::regex& exp_;
            std::string*      value_;
        } Entry;
        size_t match_count = 0;
        const std::vector<Entry> all = {
            Entry {Processor::sk_details_id_regex_  , &o_rule.id_   },
            Entry {Processor::sk_details_msg_regex_ , &o_rule.msg_  },
            Entry {Processor::sk_details_file_regex_, &o_rule.file_ },
            Entry {Processor::sk_details_data_regex_, &o_rule.data_ }
        };
        for ( auto& it : all ) {
            if ( true == std::regex_match(data, match, it.exp_) && 2 == match.size() ) {
                (*it.value_) = match[1].str();
                match_count++;
            }
        }
        if ( true == std::regex_match(data, match, Processor::sk_details_line_regex_) && 2 == match.size() ) {
            o_rule.line_ = std::stoi(match[1].str());
            match_count++;
        }
        if ( 0 == match_count ) {
            o_rule.data_ = data;
        }
    }
    // ... save code ...
    o_rule.code_ = o_intervention.status;
    // ... done ...
    return true;
}
