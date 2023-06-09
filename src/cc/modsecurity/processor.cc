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

#include "cc/types.h"
#include "cc/debug/types.h"

// MARK: - ProcessorOneShotInitializer

cc::modsecurity::ProcessorOneShotInitializer::ProcessorOneShotInitializer (cc::modsecurity::Processor& a_instance)
    : ::cc::Initializer<cc::modsecurity::Processor>(a_instance)
{
    instance_.mod_security_ = nullptr;
    instance_.rules_set_    = nullptr;
}

cc::modsecurity::ProcessorOneShotInitializer::~ProcessorOneShotInitializer ()
{
    msc_rules_cleanup(instance_.rules_set_);
    msc_cleanup(instance_.mod_security_);
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
 * @param a_path Config directory path.
 */
void cc::modsecurity::Processor::Startup (const std::string& a_path)
{
    // ... one-shot call only ...
    if ( nullptr != mod_security_ || nullptr != rules_set_ ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__);
    }
    // ... setup ...
    try {
        // ... create modsecurity instance ...
        mod_security_ = new ::modsecurity::ModSecurity();
        mod_security_->setConnectorInformation("casper-connectors");
        // ... create and load rules ...
        rules_set_ = new ::modsecurity::RulesSet();
        if ( rules_set_->loadFromUri(( a_path + "default-mod-security/modsec_includes.conf" ).c_str()) < 0 ) {
            throw ::cc::Exception("%s", rules_set_->getParserError().c_str());
        }
    } catch (...) {
        // ... clean up ...
        if ( nullptr != mod_security_ ) {
            delete mod_security_;
            mod_security_ = nullptr;
        }
        if ( nullptr != rules_set_ ) {
            delete rules_set_;
            rules_set_ = nullptr;
        }
        // ... notify ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
}

// MARK: -

/**
 * @brief Simulate an HTTP request and validate it's content.
 *
 * @param a_request Request data.
 * @param o_rule    If denied, object will be filled with rule data.
 */

#define CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(...) \
if ( true == RequiredIntervention([&] { __VA_ARGS__;  return transaction; }, it, o_rule) ) { \
        delete transaction; \
        return o_rule.code_; \
    }

int cc::modsecurity::Processor::SimulateHTTPRequest (const Processor::POSTRequest& a_request, Processor::Rule& o_rule)
{
    o_rule = { /* id_ */ "" , /* msg */ "", /* file */ "", /* line_ */ 0, /* data_ */ "", /* code_ */ 200 };
    
    // ... sanity check ...
    if ( nullptr == mod_security_ || nullptr == rules_set_ ) {
        // ... NOT initialized, report ...
        throw ::cc::Exception("modsecurity not - %s!", "initialized");
    }

    ::modsecurity::ModSecurityIntervention it;
    ::modsecurity::intervention::reset(&it);

    ::modsecurity::Transaction* transaction = new ::modsecurity::Transaction(mod_security_, rules_set_, /* logCbData */ this);
    try {
        // TODO: MODSECURITY FIX THIS - used param values
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
                                                               transaction->processConnection("127.0.0.1", 12345, "127.0.0.1", 80);
        );
        // TODO: MODSECURITY FIX THIS - comment
        CC_MODSECURITY_PROCESSOR_RUN_AND_VALIDATE_INTERVENTION(
            transaction->processURI(a_request.uri_.c_str(), a_request.protocol_.c_str(), a_request.version_.c_str());
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
        // ... clean up ...
        delete transaction;        
    } catch (const cc::Exception& a_cc_exception) {
        // ... clean up ...
        delete transaction;
        // ... and notify ...
        throw a_cc_exception;
    } catch (...) {
        // ... clean up ...
        delete transaction;
        // ... and notify ...
        ::cc::Exception::Rethrow(/* a_unhandled */ false, __FILE__, __LINE__, __FUNCTION__);
    }
    // ... done ...
    return o_rule.code_;
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
        const std::vector<Entry> all = {
            Entry {Processor::sk_details_id_regex_  , &o_rule.id_   },
            Entry {Processor::sk_details_msg_regex_ , &o_rule.msg_  },
            Entry {Processor::sk_details_file_regex_, &o_rule.file_ },
            Entry {Processor::sk_details_data_regex_, &o_rule.data_ }
        };
        for ( auto& it : all ) {
            if ( true == std::regex_match(data, match, it.exp_) && 2 == match.size() ) {
                (*it.value_) = match[1].str();
            }
        }
        if ( true == std::regex_match(data, match, Processor::sk_details_line_regex_) && 2 == match.size() ) {
            o_rule.line_ = std::stoi(match[1].str());
        }
    }
    // ... save code ...
    o_rule.code_ = o_intervention.status;
    // ... done ...
    return true;
}
