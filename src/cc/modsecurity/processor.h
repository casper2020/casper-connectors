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

#include <regex>      // std::regex

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
                const std::string& content_type_;
                const std::string& from_;
                const std::string& to_;
                const std::string& uri_;
                const std::string& protocol_;
                const std::string& version_;
                const std::string& body_file_uri_;
                const std::string& query_;
            } POSTRequest;
            
            typedef struct {
                std::string id_;
                std::string msg_;
                std::string file_;
                int         line_;
                std::string data_;
                int         code_;
            } Rule;
            
        private: // Static Const Data
            
            static const std::regex sk_details_id_regex_;
            static const std::regex sk_details_msg_regex_;
            static const std::regex sk_details_file_regex_;
            static const std::regex sk_details_line_regex_;
            static const std::regex sk_details_data_regex_;
            
        private: // Data
            
            ::modsecurity::ModSecurity* mod_security_;
            ::modsecurity::RulesSet*    rules_set_;
            std::string                 config_file_uri_;
            
        public: // Constructor(s) / Destructor
            
            Processor ();
            virtual ~Processor();
            
        public: // One-shot Call API Method(s) / Function(s)
            
            void Startup  (const std::string& a_path);
            void Shutdown ();
        
        public: // API Method(s) / Function(s)
            
            void Recycle ();
            
        public: // API Method(s) / Function(s)
            
            int SimulateHTTPRequest (const POSTRequest& a_request, Rule& o_rule);
            
        private: // Method(s) / Function(s)
            
            void Initialize ();
            void CleanUp    ();
            
        private: // Method(s) / Function(s)
            
            bool RequiredIntervention (const std::function<::modsecurity::Transaction*()>& a_callback,
                                       ::modsecurity::ModSecurityIntervention& o_intervention, Rule& o_rule);
            
        }; // end of class 'Processor'
        
    } // end of namespace 'modsecurity'

} // end of namespace 'cc'

#endif // CC_MODSECURITY_PROCESSOR_H_
