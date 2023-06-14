/**
 * @file processor.h
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

#pragma once
#ifndef CC_MODSECURITY_PROCESSOR_H_
#define CC_MODSECURITY_PROCESSOR_H_

#include "cc/singleton.h"

#include "modsecurity/modsecurity.h"
#include "modsecurity/rules_set.h"

#include "ev/logger_v2.h"

#include <regex> // std::regex
#include <map>
#include <string>

#include "cc/debug/types.h"

namespace cc
{

    namespace modsecurity
    {
        
        // ---- //
        class Processor;
        class ProcessorOneShotInitializer final : public ::cc::Initializer<cc::modsecurity::Processor>
        {
            
        public: // Constructor(s) / Destructor
            
            ProcessorOneShotInitializer (cc::modsecurity::Processor& a_instance);
            virtual ~ProcessorOneShotInitializer ();
            
        }; // end of class 'OneShotInitializer'

        // ---- //
        class Processor final : public ::cc::Singleton<Processor, ProcessorOneShotInitializer>
        {
            
            friend class ProcessorOneShotInitializer;
            
        public: // Data Type(s)
            
            typedef struct {
                std::string ip_;
                int         port_;
            } Addr;
            
            typedef struct {
                std::string                             id_;
                std::string                             module_;
                std::string                             content_type_;
                size_t                                  content_length_;
                Addr                                    client_;
                Addr                                    server_;
                std::string                             uri_;
                std::string                             version_;
                std::string                             body_file_uri_;
                std::function<void(const std::string&)> logger_;
            } HTTPPOSTRequest;
            
            typedef struct {
                std::string id_;
                std::string msg_;
                std::string file_;
                int         line_;
                std::string data_;
                int         code_;
            } Rule;

        private: // Data Type(s)
            
            typedef struct {
                size_t      padding_;
                std::string section_;
                std::string separator_;
            } LogConfig;
            
            typedef struct {
                ::modsecurity::ModSecurity* mod_security_;
                ::modsecurity::RulesSet*    rules_set_;
    CC_IF_DEBUG(::modsecurity::DebugLog*    debug_log_;)
                std::string                 config_uri_;
                LogConfig                   log_config_;
            } Instance;

        private: // Static Const Data
            
            static const std::regex sk_details_id_regex_;
            static const std::regex sk_details_msg_regex_;
            static const std::regex sk_details_file_regex_;
            static const std::regex sk_details_line_regex_;
            static const std::regex sk_details_data_regex_;
            
        private: // Data
            
            std::map<std::string, Instance*> instances_;
            
        private: // Data
            
            ::ev::Loggable::Data*    loggable_data_;
            ::ev::LoggerV2::Client*  logger_client_;
            CC_IF_DEBUG(std::string  log_dir_);
            
        public: // Constructor(s) / Destructor
            
            Processor ();
            virtual ~Processor();
            
        public: // One-shot Call API Method(s) / Function(s)
            
            void Startup  (const ::ev::Loggable::Data& a_data CC_IF_DEBUG(, const std::string& a_log_dir));
            void Enable   (const std::string& a_module, const std::string& a_path, const std::string& a_file);
            void Shutdown ();
        
        public: // API Method(s) / Function(s)
            
            inline bool IsEnabled (const std::string& a_module) const { return instances_.end() != instances_.find(a_module); }
            
        public: // API Method(s) / Function(s)
            
            void Recycle ();
            
        public: // API Method(s) / Function(s)
            
            int Validate (const HTTPPOSTRequest& a_request, Rule& o_rule);
            
        private: // Method(s) / Function(s)
            
            Instance* NewInstance (const std::string& a_module,
                                    const std::string& a_uri, const bool a_first) const;
            
            void CleanUp    ();
            void Log        (const std::string& a_module,
                             const Processor::Instance& a_instance, const bool a_first) const;
            
        private: // Method(s) / Function(s)
            
            bool RequiredIntervention (const std::function<::modsecurity::Transaction*()>& a_callback,
                                       ::modsecurity::ModSecurityIntervention& o_intervention, Rule& o_rule);
            
        }; // end of class 'Processor'
        
    } // end of namespace 'modsecurity'

} // end of namespace 'cc'

#endif // CC_MODSECURITY_PROCESSOR_H_
