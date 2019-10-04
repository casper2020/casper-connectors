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

#include "osal/thread_helper.h"

#include "ev/logger_v2.h"

namespace ev
{

    namespace auth
    {
    
        namespace route
        {

            class Gatekeeper final : public osal::Singleton<Gatekeeper> // TODO public ::cc::NonCopyable, public ::cc::NonMovable
            {
                
            public: // Data Type(s)
                
                typedef std::function<void(const std::string&)> JobDeflector;
                
            private: // Data Type(s)
                
                class Rule : public ::cc::NonCopyable, public ::cc::NonMovable {
                    
                public: // Data Type(s)
                    
                    typedef struct {
                        const std::string           tube_;
                        const std::set<std::string> methods_;
                    } Job;
                    
                    typedef struct {
                        const std::string str_;
                        const std::regex  regex_;
                    } Expression;
                    
                public: // Const Data
                    
                    const size_t                idx_;
                    const Expression            expr_;
                    const std::set<std::string> methods_;
                    const unsigned long         role_mask_;
                    const Job*                  job_;
                    
                public: // Constructor(s) / Destructor
                    
                    Rule () = delete;
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_expr      Regex.
                     * @param a_methods   Supported { "GET", "POST", "PATCH", "DELETE" }
                     * @param a_role_mask Bitmask.
                     * @param a_job       \link Job \link if any ( this object will be deleted ) .
                     */
                    Rule (const size_t& a_idx, const Expression& a_expr, const std::set<std::string>& a_methods, const unsigned long& a_role_mask,
                          const Job* a_job = nullptr)
                        : idx_(a_idx), expr_(a_expr), methods_(a_methods), role_mask_(a_role_mask), job_(a_job)
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
                
            public: // Data Type(s)
                
                typedef struct {
                    uint16_t    code_;
                    Json::Value data_;
                } Status;
                
            private: // Static Data
                
                static ::ev::Loggable::Data*   s_loggable_data_;
                static ::ev::LoggerV2::Client* s_logger_client_;
                static size_t                  s_logger_index_padding_;
                static std::string             s_logger_index_fmt_;
                static std::string             s_logger_method_fmt_;
                static std::string             s_logger_section_;
                static std::string             s_logger_separator_;
                static bool                    s_initialized_;

            private: // Data
                
                std::vector<Rule*> rules_;

                std::string        tmp_path_;
                std::smatch        tmp_match_;
                CURLU*             tmp_url_;
                std::ostringstream tmp_ostream_;
                Status             tmp_status_;

            public: // Method(s) / Functions

                void          Startup  (const Loggable::Data& a_loggable_data_ref, const std::string& a_uri);
                void          Load     (const std::string& a_uri, const size_t a_signo);
                const Status& Allow    (const char* const a_method, const std::string& a_url, const ev::casper::Session& a_session,
                                        JobDeflector a_deflector = nullptr);
                void     Shutdown ();
                
            private: // Method(s) / Function(s)
                
                void ExtractURLComponent (const std::string& a_url, const CURLUPart a_part, std::string& o_value);

            private: // Error(s) / Exception Serialization Method(s) / Function(s)

                const Status& SerializeError     (const char* const a_method, const std::string& a_path, const ev::casper::Session& a_session,
                                                  const Rule* a_rule, const uint16_t a_code);
                const Status& SerializeException (const char* const a_method, const std::string& a_path, const ev::casper::Session& a_session,
                                                  const ev::Exception& a_exception);

            private: // Logging Method(s) / Function(s)

                void Log (const Rule* a_rule);
                void Log (const char* const a_method, const uint16_t& a_status_code, const Rule* a_rule,
                          const ev::Exception* a_exception);

            }; // end of class 'Gatekeeper'

        } // end of namespace 'route'
    
    } // end of namespace 'auth'

} // end of namespace 'ev'

#endif // NRS_EV_AUTH_ROUTE_GATEKEEPER_H_
