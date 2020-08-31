/**
 * @file config.h
 *
 * Copyright (c) 2010-2017 Cloudware S.A. All rights reserved.
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
#ifndef NRS_EV_LOOP_BEANSTALKD_CONFIG_H_
#define NRS_EV_LOOP_BEANSTALKD_CONFIG_H_

#include "ev/beanstalk/config.h"
#include "ev/postgresql/config.h"
#include "ev/redis/config.h"

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {

            typedef struct {
                const std::string                  abbr_;
                const std::string                  name_;
                const std::string                  version_;
                const std::string                  rel_date_;
                const std::string                  info_;
                const std::string                  banner_;
                const int                          instance_;
                const std::string                  exec_path_;
                const std::string                  conf_file_uri_;
            } StartupConfig;

            typedef struct _SharedConfig {
                
                std::string                        ip_addr_;
                ev::Directories                    directories_;
                std::map<std::string, std::string> log_tokens_;
                ev::redis::Config                  redis_;
                ev::postgresql::Config             postgres_;
                ev::beanstalk::Config              beanstalk_;
                DeviceLimitsMap                    device_limits_;
                
                _SharedConfig(const std::string& a_ip_addr,
                              const ev::Directories& a_directories, const std::map<std::string, std::string>& a_log_tokens,
                              const ev::redis::Config& a_redis, const ev::postgresql::Config& a_postgres, const ev::beanstalk::Config& a_beanstalk, const DeviceLimitsMap& a_device_limits)
                {
                    ip_addr_       = a_ip_addr;
                    directories_   = a_directories;
                    
                    for ( auto it : a_log_tokens ) {
                        log_tokens_[it.first] = it.second;
                    }
                    
                    redis_         = a_redis;
                    postgres_      = a_postgres;
                    beanstalk_     = a_beanstalk;
                    
                    for ( auto it : a_device_limits ) {
                        device_limits_[it.first] = it.second;
                    }
                }

                inline void operator=(const _SharedConfig& a_config)
                {
                    ip_addr_       = a_config.ip_addr_;
                    directories_   = a_config.directories_;
                    
                    for ( auto it : a_config.log_tokens_ ) {
                        log_tokens_[it.first] = it.second;
                    }
                    
                    redis_         = a_config.redis_;
                    postgres_      = a_config.postgres_;
                    beanstalk_     = a_config.beanstalk_;
                    
                    for ( auto it : a_config.device_limits_ ) {
                        device_limits_[it.first] = it.second;
                    }
                }
                
            } SharedConfig;        
        
        } // end of namespace 'beanstalkd'
    
    } // end of namespace 'loop'

} // end of namespace 'ev'
        
#endif // NRS_EV_LOOP_BEANSTALKD_CONFIG_H_
