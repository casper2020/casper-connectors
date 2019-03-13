/**
 * @file runner.h
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
#ifndef NRS_EV_LOOP_BEANSTALKD_RUNNER_H_
#define NRS_EV_LOOP_BEANSTALKD_RUNNER_H_

#include "cc/non-movable.h"
#include "cc/non-copyable.h"

#include "ev/config.h"

#include "ev/redis/config.h"
#include "ev/postgresql/config.h"

#include "ev/curl/http.h"

#include "ev/object.h"
#include "ev/loop/bridge.h"
#include "ev/loop/beanstalkd/looper.h"

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Runner : public ::cc::NonMovable, public ::cc::NonCopyable
            {

            public: // typedefs

                typedef ev::loop::beanstalkd::Looper::Factories Factories;
                
                typedef struct {
                    const std::string                  name_;
                    const int                          instance_;
                    const std::string                  exec_path_;
                    const std::string                  conf_file_uri_;
                } StartupConfig;

                typedef struct _SharedConfig {
                    
                    std::string                        default_tube_;
                    std::string                        ip_addr_;
                    ev::Directories                    directories_;
                    std::map<std::string, std::string> log_tokens_;
                    ev::redis::Config                  redis_;
                    ev::postgresql::Config             postgres_;
                    ev::beanstalk::Config              beanstalk_;
                    DeviceLimitsMap                    device_limits_;
                    Factories                          factories_;
                    
                    _SharedConfig(const std::string& a_default_tube, const std::string& a_ip_addr,
                                  const ev::Directories& a_directories, const std::map<std::string, std::string>& a_log_tokens,
                                  const ev::redis::Config& a_redis, const ev::postgresql::Config& a_postgres, const ev::beanstalk::Config& a_beanstalk, const DeviceLimitsMap& a_device_limits,
                                  const Factories& a_factories)
                    {
                        default_tube_  = a_default_tube;
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
                        
                        for ( auto it : a_factories ) {
                            factories_[it.first] = it.second;
                        }
                    }
                    
                    _SharedConfig (const _SharedConfig& a_config)
                    {
                        *this = a_config;
                    }
                    
                    inline void operator=(const _SharedConfig& a_config)
                    {
                        default_tube_  = a_config.default_tube_;
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
                        
                        for ( auto it : a_config.factories_ ) {
                            factories_[it.first] = it.second;
                        }
                    }
                    
                } SharedConfig;
                
                typedef std::function<void(const ev::Exception& a_ev_exception)> FatalExceptionCallback;

            private: // Data
                
                bool                     s_initialized_;
                bool                     s_shutting_down_;
                volatile bool            s_quit_;
                ev::loop::Bridge*        s_bridge_;
                std::thread*             s_consumer_thread_;
                osal::ConditionVariable* s_consumer_cv_;
                StartupConfig*           startup_config_;
                SharedConfig*            shared_config_;
                
                FatalExceptionCallback   s_fatal_exception_callback_;
                ev::Loggable::Data*      loggable_data_;

            protected: // Helpers
                
                ev::loop::Bridge::CallOnMainThreadCallback call_on_main_thread_;
                ev::curl::HTTP*                            http_;
                
            private: //
                
                ev::loop::beanstalkd::Looper* looper_;
                
            public: // Constructor(s) / Destructor
                
                Runner ();
                virtual ~Runner();
                
            public: // Method(s) / Function(s)
                
                void Startup (const StartupConfig& a_config,
                              FatalExceptionCallback a_fatal_exception_callback);
                void Run      ();
                void Shutdown (int a_sig_no);
                
            public: // Inline Method(s) / Function(s)
                
                void                      Quit          ();
                const ev::Loggable::Data& loggable_data () const;
                
            protected: // Pure Method(s) / Function(s)
                
                virtual void InnerStartup  (const StartupConfig& a_startup_config, const Json::Value& a_config, SharedConfig& o_config) = 0;
                virtual void InnerShutdown () = 0;
                
            private: // Method(s) / Function(s)
                
                void ConsumerLoop ();

            }; // end of class 'Runner'
            
            inline void Runner::Quit ()
            {
                s_quit_ = true;
            }
            
            inline const ev::Loggable::Data& Runner::loggable_data () const
            {
                return *loggable_data_;
            }
            
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_RUNNER_H_
