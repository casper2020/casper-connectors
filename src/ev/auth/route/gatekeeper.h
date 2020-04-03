/**
 * @file gatekeeper.h
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

#pragma once
#ifndef NRS_EV_AUTH_ROUTE_GATEKEEPER_H_
#define NRS_EV_AUTH_ROUTE_GATEKEEPER_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "ev/casper/session.h"

#include <string>
#include <vector>
#include <regex>   // std::regex

#include <curl/curl.h>
#include <curl/urlapi.h>

#include "ev/logger_v2.h"

namespace ev
{

    namespace auth
    {
    
        namespace route
        {

            class Gatekeeper final : public osal::Singleton<Gatekeeper>
            {
                
            public: // Data Type(s)
                
                typedef struct {
                    const size_t      ttr_;
                    const size_t      validity_;
                    uint16_t          status_code_;   // HTTP STATUS CODE
                    std::string       status_message_;
                } DeflectorResult;
                
                typedef struct {
                    std::function<DeflectorResult(const std::string&, ssize_t, ssize_t)> on_deflect_;
                    std::function<void(const::ev::Exception&)>                           on_exception_caught_;
                } Deflector;

            private: // Data Type(s)
                
                class Rule : public ::cc::NonCopyable, public ::cc::NonMovable {
                    
                public: // Data Type(s)
                    
                    typedef struct {
                        const std::string           tube_;
                        const ssize_t               ttr_;
                        const ssize_t               validity_;
                        const std::set<std::string> methods_;
                    } Job;
                    
                    typedef struct {
                        const std::string str_;
                        const std::regex  regex_;
                    } Expression;
                    
                    typedef struct {
                        const std::string name_;
                        const std::string got_;
                        const std::string expected_;
                        const std::string reason_;
                    } Value;
                    
                    typedef std::vector<Value> Fields;
                    
                public: // Const Data
                    
                    const size_t                idx_;
                    const Expression            expr_;
                    const std::set<std::string> methods_;
                    const unsigned long         role_mask_;
                    const unsigned long         module_mask_;
                    const Job*                  job_;
                    
                public: // Constructor(s) / Destructor
                    
                    Rule () = delete;
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_expr        Regex.
                     * @param a_methods     Supported { "GET", "POST", "PATCH", "DELETE" }
                     * @param a_role_mask   Bitmask.
                     * @param a_module_mask Bitmask.
                     * @param a_job         \link Job \link if any ( this object will be deleted ) .
                     */
                    Rule (const size_t& a_idx, const Expression& a_expr, const std::set<std::string>& a_methods,
                          const unsigned long& a_role_mask, const unsigned long& a_module_mask,
                          const Job* a_job = nullptr)
                        : idx_(a_idx), expr_(a_expr), methods_(a_methods), role_mask_(a_role_mask), module_mask_(a_module_mask), job_(a_job)
                    {
                        /* empty */
                    }
                    
                    virtual ~Rule ()
                    {
                        if ( nullptr != job_ ) {
                            delete job_;
                        }
                    }
                    
                };
                
                typedef struct {
                    std::set<std::string> bypass_methods_;
                } Bribe;
                
                typedef struct {
                    ::ev::Loggable::Data*   data_;
                    ::ev::LoggerV2::Client* client_;
                    size_t                  index_padding_;
                    std::string             index_fmt_;
                    std::string             method_fmt_;
                    std::string             section_;
                    std::string             separator_;
                    bool                    log_access_granted_;
                } LoggerSettings;
                
            public: // Data Type(s)
                
                typedef struct {
                    uint16_t       code_;
                    Json::Value    data_;
                    bool           deflected_;
                    uint16_t       deflector_code_;
                    std::string    deflector_msg_;
                    cc::Exception* exception_;
                } Status;
                
            private: // Static Data
                
                static LoggerSettings s_logger_settings_;
                static std::string    s_config_uri_;
                static bool           s_initialized_;

            private: // Data
                
                std::vector<Rule*>    rules_;

                std::string           tmp_path_;
                std::smatch           tmp_match_;
                CURLU*                tmp_url_;
                Status                status_;
                Bribe                 bribe_;

            public: // Method(s) / Functions

                void  Startup  (const Loggable::Data& a_loggable_data_ref,
                                const std::string& a_uri);
                bool  Reload   (int a_signo);
                void  Shutdown ();
                
            public: // Method(s) / Functions
                
                const Status& Allow (const std::string& a_method, const std::string& a_url, const ev::casper::Session& a_session,
                                    const Loggable::Data &a_loggable_data);
                const Status& Allow (const std::string& a_method, const std::string& a_url, const ev::casper::Session& a_session,
                                    Deflector a_deflector,
                                    const Loggable::Data &a_loggable_data);

            private: // Method(s) / Function(s)

                void Load                (const std::string& a_uri, const size_t a_signo);

                void ExtractURLComponent (const std::string& a_url, const CURLUPart a_part, std::string& o_value);

            private: // Serialization Method(s) / Function(s)
                
                const Status& SetAllowed         (const std::string& a_method, const std::string& a_path,
                                                  const Rule* a_rule);
                
                const Status& SetDeflected       (const std::string& a_method, const std::string& a_path,
                                                  const Rule* a_rule, const DeflectorResult& a_result);

                const Status& SerializeError     (const std::string& a_method, const std::string& a_path, const uint16_t a_code,
                                                  const Rule* a_rule, const Rule::Fields& a_fields,
                                                  const DeflectorResult* a_deflector_result = nullptr);
                
                const Status& SerializeException (const std::string& a_method, const std::string& a_path, const ev::Exception& a_exception);

            private: // Logging Method(s) / Function(s)

                void Log (const Rule* a_rule) const;
                void Log (const std::string& a_method, const std::string& a_path,  const uint16_t& a_status_code,
                          const Rule* a_rule, const Rule::Fields& a_fields, const DeflectorResult* a_result,
                          const ev::Exception* a_exception) const;
                void Log (const ev::Exception& a_exception);
                
            }; // end of class 'Gatekeeper'

        } // end of namespace 'route'
    
    } // end of namespace 'auth'

} // end of namespace 'ev'

#endif // NRS_EV_AUTH_ROUTE_GATEKEEPER_H_
