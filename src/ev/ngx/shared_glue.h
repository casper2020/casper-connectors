/**
 * @file shared_glue.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_EV_NGX_SHARED_GLUE_H_
#define NRS_EV_NGX_SHARED_GLUE_H_

#include <string>     // std::string
#include <map>        // std::map
#include <functional> // std::function

#include "ev/object.h"
#include "ev/ngx/includes.h"

#include "ev/beanstalk/config.h"

#include "osal/condition_variable.h"

#include "json/json.h"

namespace ev
{
    
    namespace ngx
    {
        
        class SharedGlue
        {
            
        protected: // Data Type(s)
            
            typedef struct _DeviceLimits {
                size_t                   max_conn_per_worker_;
                ssize_t                  max_queries_per_conn_;
                ssize_t                  min_queries_per_conn_;
                std::function<ssize_t()> rnd_queries_per_conn_;
            } DeviceLimits;
            
        protected: // Data
            
            std::map<ev::Object::Target, DeviceLimits> device_limits_;
            std::map<std::string, std::string>         config_map_;
            Json::Value                                postgresql_post_connect_queries_;
            
        protected: // Static Data
            
            static std::string                         s_service_id_;
            static std::string                         s_job_id_key_;
            static ::ev::beanstalk::Config             s_beanstalkd_config_;
            
        protected: // Threading
            
            osal::ConditionVariable scheduler_cv_;
            
        private: // Data
            
            std::string socket_files_dn_;
            
        public: // Constructor(s) / Destructor
            
            virtual ~SharedGlue();
            
        public: // Method(s) / Function(s)
            
            void PreConfigure     (const ngx_core_conf_t* a_config, const bool a_master);
            void PreWorkerStartup (std::string& o_scheduler_socket_fn, std::string& o_shared_handler_socket_fn);
            
            const ::ev::beanstalk::Config& BeanstalkdConfig () const;
            const std::string&             ServiceID        () const;
            const std::string&             JobIDKey         () const;
            
        protected: // Method(s) / Function(s)
            
            virtual void SetupService (const std::map<std::string, std::string>& a_config,
                                       const char* const a_service_id_key);
            
            virtual void SetupPostgreSQL (const std::map<std::string, std::string>& a_config,
                                          const char* const a_conn_str_key, const char* const a_statement_timeout_key,
                                          const char* const a_max_conn_per_worker_key,
                                          const char* const a_min_queries_per_conn_key, const char* const a_max_queries_per_conn_key,
                                          const char* const a_postgresql_post_connect_queries_key);
            
            virtual void SetupREDIS      (const std::map<std::string, std::string>& a_config,
                                          const char* const a_ip_address_key,
                                          const char* const a_port_number_key,
                                          const char* const a_database_key,
                                          const char* const a_max_conn_per_worker);
            
            virtual void SetupCURL      (const std::map<std::string, std::string>& a_config,
                                         const char* const a_max_conn_per_worker);
            
            virtual void SetupBeanstalkd (const std::map<std::string, std::string>& a_config,
                                          const char* const a_beanstalkd_host_key,
                                          const char* const a_beanstalkd_port_key,
                                          const char* const a_beanstalkd_timeout_key,
                                          const char* const a_beanstalkd_sessionless_tubes_key,
                                          const char* const a_beanstalkd_action_tubes_key,
                                          ::ev::beanstalk::Config& o_config);
            
        }; // end of class 'SharedGlue'

        /**
         * @return RO beanstalkd config.
         */
        inline const ::ev::beanstalk::Config& SharedGlue::BeanstalkdConfig () const
        {
            return s_beanstalkd_config_;
        }
        
        /**
         * @return RO service id.
         */
        inline const std::string& SharedGlue::ServiceID () const
        {
            return s_service_id_;
        }
        
        /**
         * @return RO job id key for current service.
         */
        inline const std::string& SharedGlue::JobIDKey () const
        {
            return s_job_id_key_;
        }

    } // end of namespace 'ngx'
    
} // end of namespace 'ev'

#endif // NRS_EV_SHARED_GLUE_H_
