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

#include "cc/types.h"
#include "cc/debug/types.h"

// MARK: - ProcessorOneShotInitializer

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::modsecurity::ProcessorOneShotInitializer::ProcessorOneShotInitializer (cc::modsecurity::Processor& a_instance)
    : ::cc::Initializer<cc::modsecurity::Processor>(a_instance)
{
    instance_.mod_security_   = nullptr;
    instance_.rules_set_      = nullptr;
    instance_.loggable_data_  = nullptr;
    instance_.logger_client_  = nullptr;
    instance_.logger_padding_ = 0;
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
 * @param a_data    Loggable data to copy.
 * @param a_path    Config directory path.
 * @param a_file    Main config file.
 */
void cc::modsecurity::Processor::Startup (const ::ev::Loggable::Data& a_data,
                                          const std::string& a_path, const std::string& a_file)
{
    // ... one-shot call only ...
    if ( nullptr != mod_security_ || nullptr != rules_set_ ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__);
    }
    // ... keep track of config data ...
    config_file_uri_  = cc::fs::Dir::Normalize(a_path) + a_file;
    // ... load config ...
    if ( false == cc::fs::File::Exists(config_file_uri_) ) {
        throw ::cc::Exception("Configuration file %s not found!", config_file_uri_.c_str());
    }
    // ... new instance ...
    Initialize(&a_data);
}

/**
 * @brief Shutdown processor, one-shot call only!
 */
void cc::modsecurity::Processor::Shutdown ()
{
    CleanUp();
}

/**
 * @brief Recycle processor, by reloading rules.
 */
void cc::modsecurity::Processor::Recycle ()
{
    // ... clean up ...
    CleanUp(/* a_final */ false);
    // ... and initialize ...
    Initialize(nullptr);
}

// MARK: -

/**
 * @brief Simulate an HTTP request and validate it's content.
 *
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
int cc::modsecurity::Processor::SimulateHTTPRequest (const Processor::POSTRequest& a_request, Processor::Rule& o_rule,
                                                     std::function<void(const std::string&)> a_logger)
{
    // ... reset ...
    o_rule = { /* id_ */ "" , /* msg */ "", /* file */ "", /* line_ */ 0, /* data_ */ "", /* code_ */ 200 };
    // ... sanity check ...
    if ( nullptr == mod_security_ || nullptr == rules_set_ || nullptr == loggable_data_ ) {
        // ... NOT initialized, report ...
        throw ::cc::Exception("modsecurity not - %s!", "initialized");
    }

    // ... prepare log ...
    auto _logger = [&] (const std::string& a_line) {
        ::ev::LoggerV2::GetInstance().Log(logger_client_, "cc-modsecurity", "%s", a_line.c_str());
        if ( nullptr != a_logger ) {
            a_logger(a_line);
        }
    };

    // ... update loggable data ( loggable_data_ is mutable and should be always updated  ) ...
    loggable_data_->Update(loggable_data_->module(), a_request.client_.ip_, a_request.tag_);
    
    // ... log ...
    _logger("scanning...");
    _logger("   " + a_request.content_type_ + ", " + std::to_string(a_request.content_length_) + " byte(s)");
    _logger("   file: " + a_request.body_file_uri_);
    
    // ... process ...
    ::modsecurity::Transaction* transaction = new ::modsecurity::Transaction(mod_security_, rules_set_, /* logCbData */ this);
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
            ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
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
 */
void cc::modsecurity::Processor::Initialize (const ::ev::Loggable::Data* a_data)
{
    // ... sanity check ...
    CC_ASSERT(nullptr == mod_security_ && nullptr == rules_set_ && 0 != config_file_uri_.length());
    // ...
    try {
        logger_padding_   = std::max<size_t>(config_file_uri_.length(), 106);
        logger_section_   = "--- " + std::string(logger_padding_, ' ') + " ---";
        logger_separator_ = "--- " + std::string(logger_padding_, '-') + " ---";
        // ... new instances ...
        if ( nullptr == loggable_data_ && nullptr != a_data ) {
            loggable_data_ = new ::ev::Loggable::Data(*a_data);
        }
        if ( nullptr == logger_client_ && nullptr != loggable_data_ ) {
            logger_client_ = new ::ev::LoggerV2::Client(*loggable_data_);
            ::ev::LoggerV2::GetInstance().Register(logger_client_, { "cc-modsecurity" });
        }
        // ... create new modsecurity instance ...
        mod_security_ = new ::modsecurity::ModSecurity();
        mod_security_->setConnectorInformation("casper-connectors");
        // ... create and load new rules ...
        rules_set_ = new ::modsecurity::RulesSet();
        if ( rules_set_->loadFromUri(config_file_uri_.c_str()) < 0 ) {
            throw ::cc::Exception("%s", rules_set_->getParserError().c_str());
        }
        // ... log ...
        Log((nullptr == a_data ? SIGTTIN : 0));
    } catch (...) {
        // ... clean up ...
        CleanUp(/* a_final */ false);
        // ... notify ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
}

/**
 * @brief Release previously allocated memory.
 *
 * @param a_final When false, loggging data will be preserved.
 */
void cc::modsecurity::Processor::CleanUp (const bool a_final)
{
    if ( nullptr != rules_set_ ) {
        delete rules_set_;
        rules_set_ = nullptr;
    }
    if ( nullptr != mod_security_ ) {
        delete mod_security_;
        mod_security_ = nullptr;
    }
    if ( true == a_final ) {
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
}

/**
 * @brief Load rules.
 *
 * @param a_signo Signal number, 0 if it's a startup log.
 */
void cc::modsecurity::Processor::Log (const int a_signo) const
{
    // ... update loggable data ( loggable_data_ is mutable and should be always updated  ) ...
    loggable_data_->Update(/* a_module */ "cc-modsecurity", /* a_ip_addr */ "", /* a_tag */ ( 0 == a_signo ? "startup" : "signal"));
        
    auto& logger = ::ev::LoggerV2::GetInstance();
    
    // ... present ...
    std::vector<std::string> log_lines;
    log_lines.push_back(logger_separator_);
    log_lines.push_back(logger_section_);
    log_lines.push_back("--- " + cc::UTCTime::NowISO8601WithTZ());
    log_lines.push_back("--- " + config_file_uri_);
    log_lines.push_back(logger_section_);
    logger.Log(logger_client_, "cc-modsecurity", log_lines);

    // ... log minimalist rules data ...
    size_t number_of_rules = 0;
    for ( int phase_no = 0 ; phase_no < ::modsecurity::Phases::NUMBER_OF_PHASES; phase_no++ ) {
        const auto rs = rules_set_->m_rulesSetPhases.at(phase_no);
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
    logger.Log(logger_client_, "cc-modsecurity", "%s", logger_section_.c_str());
    logger.Log(logger_client_, "cc-modsecurity", SIZET_FMT " rule(s) loaded", number_of_rules);
    logger.Log(logger_client_, "cc-modsecurity", "%s", logger_separator_.c_str());    
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
