/**
 * @file shared_glue.cc
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

#include "ev/ngx/shared_glue.h"

#include "osal/utf8_string.h"
#include "osal/debug/trace.h"

#include "cc/logs/basic.h"

#include "ev/exception.h"

#include "ev/logger.h"
#include "ev/logger_v2.h"

#include <sstream> // std::stringstream

#include <sys/stat.h> // chmod
#include <unistd.h>  // chown

std::string ev::ngx::SharedGlue::s_service_id_ = "";
std::string ev::ngx::SharedGlue::s_job_id_key_ = "";

::ev::beanstalk::Config ev::ngx::SharedGlue::s_beanstalkd_config_ = {
    /* host_              */ "127.0.0.1",
    /* port_              */  11300,
    /* timeout_           */  0.0,
    /* abort_polling_     */ 3,
    /* tubes_             */ {
        "default"
    },    
    /* sessionless_tubes_ */ {},
    /* action_tubes_      */ {}
};

/**
 * @brief Destructor.
 */
ev::ngx::SharedGlue::~SharedGlue ()
{
    /* empty */
}

/**
 * @brief Shared pre-startup configuration.
 *
 * @param a_config The 'master' process config struct..
 * @param a_main   True when called from master process, false otherwise.
 */
void ev::ngx::SharedGlue::PreConfigure (const ngx_core_conf_t* a_config, const bool a_master)
{
    
    const std::string master_pid_file = std::string(reinterpret_cast<char const*>(a_config->pid.data), a_config->pid.len);

    socket_files_dn_ = osal::UTF8StringHelper::ReplaceAll(master_pid_file, ".pid", "");

	// ... ensure directory exists ...
    if ( osal::posix::Dir::EStatusOk != osal::Dir::Exists(socket_files_dn_.c_str()) ) {
        const osal::Dir::Status mkdir_status = osal::Dir::CreateDir(socket_files_dn_.c_str());
        if ( mkdir_status != osal::Dir::EStatusOk ) {
			const int last_error = errno;
            throw ev::Exception("Unable to create sockets directory '%s'~ %d ~ %s!",
                                socket_files_dn_.c_str(), last_error, strerror(last_error)
            );
        }
    }

    // ... ensure directory can be written to ...
    if ( UINT32_MAX != a_config->user && UINT32_MAX != a_config->group ) {
        
        ev::Logger::GetInstance().EnsureOwnership(a_config->user, a_config->group);
        ev::LoggerV2::GetInstance().EnsureOwnership(a_config->user, a_config->group);
        cc::logs::Basic::GetInstance().EnsureOwnership(a_config->user, a_config->group);

	
        osal::debug::Trace::GetInstance().EnsureOwnership(a_config->user, a_config->group);
        
        const int chown_status = chown(socket_files_dn_.c_str(), a_config->user, a_config->group);
        if ( 0 != chown_status ) {
			const int last_error = errno;
            throw ev::Exception("Unable to change sockets directory '%s'owner to %u:%u ~ %d ~ %s!",
                                socket_files_dn_.c_str(), a_config->user, a_config->group, last_error, strerror(last_error)
            );
        }
    }
	
    // ... from now on, directory name ends width '/' ...
    socket_files_dn_ += "/";

    // ... release 'old' socket files?
    if ( true == a_master ) {
        osal::File::Delete(socket_files_dn_.c_str(), "ev-*.socket", nullptr);
    }
}

/**
 * @brief Must be called per worker and before it's startup step, so socket file names can be generated.
 *
 * @param o_scheduler_socket_fn
 * @param o_shared_handler_socket_fn
 */
void ev::ngx::SharedGlue::PreWorkerStartup (std::string& o_scheduler_socket_fn, std::string& o_shared_handler_socket_fn)
{
    std::stringstream ss;
    const pid_t pid = getpid();
    
    ss.str("");
    ss << socket_files_dn_ << "ev-scheduler-" << pid << ".socket";
    o_scheduler_socket_fn = ss.str();
    
    ss.str("");
    ss << socket_files_dn_ << "ev-shared-handler-" << pid << ".socket";
    o_shared_handler_socket_fn = ss.str();
}

/**
 * @brief Setup service.
 *
 * @param a_config
 * @param a_service_id_key
 */
void ev::ngx::SharedGlue::SetupService (const std::map<std::string, std::string>& a_config,
                                        const char* const a_service_id_key)
{
    const auto service_id_it = a_config.find(a_service_id_key);
    if ( a_config.end() != service_id_it ) {
        s_service_id_ = service_id_it->second;
        s_job_id_key_ = s_service_id_ + ":jobs:sequential_id";
    }
}

/**
 * @brief Setup PostgreSQL devices properties.
 *
 * @param a_config
 * @param a_conn_str_key
 * @param a_statement_timeout_key
 * @param a_max_conn_per_worker_key
 * @param a_min_queries_per_conn_key
 * @param a_max_queries_per_conn_key
 * @param a_postgresql_post_connect_queries_key;
 */ 
void ev::ngx::SharedGlue::SetupPostgreSQL (const std::map<std::string, std::string>& a_config,
                                           const char* const a_conn_str_key, const char* const a_statement_timeout_key,
                                           const char* const a_max_conn_per_worker_key,
                                           const char* const a_min_queries_per_conn_key, const char* const a_max_queries_per_conn_key,
                                           const char* const a_post_connect_queries_key)
{
    
    const std::map<std::string, std::string> map = {
        { a_conn_str_key          , ""    },
        { a_statement_timeout_key , "300" },
    };
    for ( auto entry : map ) {
        const auto it = a_config.find(entry.first);
        if ( a_config.end() != it ) {
            config_map_[entry.first] = it->second;
        } else {
            config_map_[entry.first] = entry.second;
        }
    }
    
    const auto postgresql_conn_str_it = a_config.find(a_conn_str_key);
    if ( a_config.end() != postgresql_conn_str_it ) {
        config_map_[a_conn_str_key] = postgresql_conn_str_it->second;
    }

    const auto postgresql_statement_timeout = a_config.find(a_statement_timeout_key);
    if ( a_config.end() != postgresql_statement_timeout ) {
        config_map_[a_statement_timeout_key] = postgresql_statement_timeout->second;
    }

    size_t postgresql_max_conn_per_worker = 1;

    const auto postgresql_max_conn_per_worker_it = a_config.find(a_max_conn_per_worker_key);
    if ( a_config.end() != postgresql_max_conn_per_worker_it ) {
        postgresql_max_conn_per_worker = static_cast<size_t>(std::max(std::stoi(postgresql_max_conn_per_worker_it->second), 1));
    }
    
    
    ssize_t postgresql_min_queries_per_conn = -1;
    if ( nullptr != a_min_queries_per_conn_key ) {
        const auto postgresql_min_queries_per_conn_it = a_config.find(a_min_queries_per_conn_key);
        if ( a_config.end() != postgresql_min_queries_per_conn_it ) {
            postgresql_min_queries_per_conn = std::max((ssize_t)std::stoi(postgresql_min_queries_per_conn_it->second), (ssize_t)-1);
        }
    }

    ssize_t postgresql_max_queries_per_conn = -1;
    if ( nullptr != a_max_queries_per_conn_key ) {
        const auto postgresql_max_queries_per_conn_it = a_config.find(a_max_queries_per_conn_key);
        if ( a_config.end() != postgresql_max_queries_per_conn_it ) {
            postgresql_max_queries_per_conn = std::max((ssize_t)std::stoi(postgresql_max_queries_per_conn_it->second), (ssize_t)-1);
        }
        
    }

    if ( postgresql_min_queries_per_conn > postgresql_max_queries_per_conn ){
        ssize_t tmp = postgresql_max_queries_per_conn;
        postgresql_max_queries_per_conn = postgresql_min_queries_per_conn;
        postgresql_min_queries_per_conn = tmp;
    }

    device_limits_[::ev::Object::Target::PostgreSQL] = {
        /* max_conn_per_worker_  */ postgresql_max_conn_per_worker,
        /* max_queries_per_conn_ */ postgresql_max_queries_per_conn,
        /* min_queries_per_conn_ */ postgresql_min_queries_per_conn,
        /* rnd_queries_per_conn_ */ [this] () -> ssize_t {
            
            ssize_t max_queries_per_conn = -1;
            
            const auto limits_it = device_limits_.find(ev::Object::Target::PostgreSQL);
            if ( device_limits_.end() != limits_it ) {
                
                if ( limits_it->second.min_queries_per_conn_ > -1 && limits_it->second.max_queries_per_conn_ > -1 ) {
                    // ... both limits are set ...
                    max_queries_per_conn = limits_it->second.min_queries_per_conn_ +
                    (
                     random() % ( limits_it->second.max_queries_per_conn_ - limits_it->second.min_queries_per_conn_ + 1 )
                     );
                } else if ( -1 == limits_it->second.min_queries_per_conn_ && limits_it->second.max_queries_per_conn_ > -1  ) {
                    // ... only upper limit is set ...
                    max_queries_per_conn = limits_it->second.max_queries_per_conn_;
                } // else { /* ignored */ }
                
            }
            
            return max_queries_per_conn;
            
        }
    };
    
    if ( nullptr != a_post_connect_queries_key ) {
        const auto postgresql_post_connect_queries_it = a_config.find(a_post_connect_queries_key);
        if ( a_config.end() != postgresql_post_connect_queries_it ) {
            Json::Reader reader;
            if ( false == reader.parse(postgresql_post_connect_queries_it->second, postgresql_post_connect_queries_) ) {
                throw ev::Exception("Unable to parse %s value - expected valid JSON string!", a_post_connect_queries_key);
            }
        }    
    }
}

/**
 * @brief Setup REDIS devices properties.
 *
 * @param a_config
 * @param a_ip_address_key
 * @param a_port_number_key
 * @param a_database_key
 * @param a_max_conn_per_worker
 */
void ev::ngx::SharedGlue::SetupREDIS (const std::map<std::string, std::string>& a_config,
                                      const char* const a_ip_address_key,
                                      const char* const a_port_number_key,
                                      const char* const a_database_key,
                                      const char* const a_max_conn_per_worker)
{
    
    const std::map<std::string, std::string> map = {
        { a_ip_address_key         , ""    },
        { a_port_number_key        , ""    },
        { a_database_key           , "-1"  },
    };
    for ( auto entry : map ) {
        const auto it = a_config.find(entry.first);
        if ( a_config.end() != it ) {
            config_map_[entry.first] = it->second;
        } else {
            config_map_[entry.first] = entry.second;
        }
    }

    const auto redis_ip_address_it = a_config.find(a_ip_address_key);
    if ( a_config.end() != redis_ip_address_it ) {
        config_map_[a_ip_address_key] = redis_ip_address_it->second;
    }
    
    const auto redis_port_number_it = a_config.find(a_port_number_key);
    if ( a_config.end() != redis_port_number_it ) {
        config_map_[a_port_number_key] = redis_port_number_it->second;
    }
    
    const auto redis_database_it = a_config.find(a_database_key);
    if ( a_config.end() != redis_database_it ) {
        config_map_[a_database_key] = redis_database_it->second;
    }
    size_t redis_max_conn_per_worker = 1;
    
    const auto redis_max_conn_per_worker_it = a_config.find(a_max_conn_per_worker);
    if ( a_config.end() != redis_max_conn_per_worker_it ) {
        redis_max_conn_per_worker = static_cast<size_t>(std::max(std::stoi(redis_max_conn_per_worker_it->second), static_cast<int>(redis_max_conn_per_worker)));
    }
    
    device_limits_[::ev::Object::Target::Redis] = {
        /* max_conn_per_worker_  */ redis_max_conn_per_worker,
        /* max_queries_per_conn_ */ -1,
        /* min_queries_per_conn_ */ -1,
        /* rnd_queries_per_conn_ */ nullptr
    };
}

/**
 * @brief Setup REDIS devices properties.
 *
 * @param a_max_conn_per_worker
 */
void ev::ngx::SharedGlue::SetupCURL (const std::map<std::string, std::string>& a_config,
                                     const char* const a_max_conn_per_worker)
{
    size_t curl_max_conn_per_worker = 1;
    
    const auto curl_max_conn_per_worker_it = a_config.find(a_max_conn_per_worker);
    if ( a_config.end() != curl_max_conn_per_worker_it ) {
        curl_max_conn_per_worker = static_cast<size_t>(std::max(std::stoi(curl_max_conn_per_worker_it->second), static_cast<int>(curl_max_conn_per_worker)));
    }
    
    device_limits_[::ev::Object::Target::CURL] = {
        /* max_conn_per_worker_  */ curl_max_conn_per_worker,
        /* max_queries_per_conn_ */ -1,
        /* min_queries_per_conn_ */ -1,
        /* rnd_queries_per_conn_ */ nullptr
    };
}

/**
 * @brief Setup beanstalkd.
 *
 * @param a_config
 * @param a_beanstalkd_host_key
 * @param a_beanstalkd_port_key
 * @param a_beanstalkd_timeout_key
 * @param a_beanstalkd_sessionless_tubes_key
 * @param a_beanstalkd_action_tubes_key
 * @param o_config
 */
void ev::ngx::SharedGlue::SetupBeanstalkd (const std::map<std::string, std::string>& a_config,
                                           const char* const a_beanstalkd_host_key,
                                           const char* const a_beanstalkd_port_key,
                                           const char* const a_beanstalkd_timeout_key,
                                           const char* const a_beanstalkd_sessionless_tubes_key,
                                           const char* const a_beanstalkd_action_tubes_key,
                                           ::ev::beanstalk::Config& o_config)
{
    // ... beanstalkd ...
    const auto beanstalkd_host_it = a_config.find(a_beanstalkd_host_key);
    if ( a_config.end() != beanstalkd_host_it ) {
        o_config.host_ = beanstalkd_host_it->second;
    }
    const auto beanstalkd_port_it = a_config.find(a_beanstalkd_port_key);
    if ( a_config.end() != beanstalkd_port_it ) {
        o_config.port_ = std::atoi(beanstalkd_port_it->second.c_str());
    }
    const auto beanstalkd_timeout_it = a_config.find(a_beanstalkd_timeout_key);
    if ( a_config.end() != beanstalkd_timeout_it ) {
        o_config.timeout_ = (float)std::atof(beanstalkd_timeout_it->second.c_str());
    }
    // ... sessionless, action tubes, an array of strings is expected ...
    const std::map<const char* const, std::set<std::string>*> beanstalkd_tubes_map = {
        { a_beanstalkd_sessionless_tubes_key, &o_config.sessionless_tubes_ },
        { a_beanstalkd_action_tubes_key     , &o_config.action_tubes_      }
    };
    Json::Reader reader;
    for ( auto beanstalkd_tubes_it : beanstalkd_tubes_map ) {
        const auto value_it = a_config.find(beanstalkd_tubes_it.first);
        if ( a_config.end() == value_it ) {
            continue;
        }
        Json::Value  array;
        if ( false == reader.parse(value_it->second.c_str(), array) ) {
            throw ev::Exception("Unable to parse %s value - expected valid JSON string!", value_it->first.c_str());
        }
        for ( Json::ArrayIndex idx = 0 ; idx < array.size() ; ++idx ) {
            beanstalkd_tubes_it.second->insert(array[idx].asString());
        }
    }
}

