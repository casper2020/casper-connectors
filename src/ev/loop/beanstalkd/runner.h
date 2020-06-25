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

#include "cc/global/types.h"

#include "ev/config.h"

#include "ev/redis/config.h"
#include "ev/postgresql/config.h"

#include "ev/curl/http.h"

#include "ev/object.h"
#include "ev/loop/bridge.h"
#include "ev/loop/beanstalkd/looper.h"

#include <mutex>

namespace ev
{
    
    namespace loop
    {
        
        namespace beanstalkd
        {
            
            class Runner : public ::cc::NonMovable, public ::cc::NonCopyable
            {

            public: // typedefs

                typedef ev::loop::beanstalkd::Job::Factory Factory;
                typedef std::function<void(const ev::Exception& a_ev_exception)> FatalExceptionCallback;

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
                    
                    std::string                        default_tube_;
                    std::string                        ip_addr_;
                    ev::Directories                    directories_;
                    std::map<std::string, std::string> log_tokens_;
                    ev::redis::Config                  redis_;
                    ev::postgresql::Config             postgres_;
                    ev::beanstalk::Config              beanstalk_;
                    DeviceLimitsMap                    device_limits_;
                    Factory                            factory_;
                    
                    _SharedConfig(const std::string& a_default_tube, const std::string& a_ip_addr,
                                  const ev::Directories& a_directories, const std::map<std::string, std::string>& a_log_tokens,
                                  const ev::redis::Config& a_redis, const ev::postgresql::Config& a_postgres, const ev::beanstalk::Config& a_beanstalk, const DeviceLimitsMap& a_device_limits,
                                  const Factory& a_factory)
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
                        
                        factory_ = a_factory;
                        
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
                        
                        factory_ = a_config.factory_;
                    }
                    
                } SharedConfig;
                
            private: // Data
                
                bool                     initialized_;
                bool                     shutting_down_;
                volatile bool            quit_;
                ev::loop::Bridge*        bridge_;
                std::thread*             consumer_thread_;
                osal::ConditionVariable* consumer_cv_;
                StartupConfig*           startup_config_;
                SharedConfig*            shared_config_;                
                ev::Loggable::Data*      loggable_data_;

            private: // Helpers
                
                ev::curl::HTTP*          http_;
                
            private: // Callbacks
                
                ev::loop::beanstalkd::Runner::FatalExceptionCallback on_fatal_exception_;
                
            private: // Threading Ptrs
                
                std::mutex                    looper_mutex_;
                ev::loop::beanstalkd::Looper* looper_ptr_;
                
            public: // Constructor(s) / Destructor
                
                Runner ();
                virtual ~Runner();
                
            public: // Method(s) / Function(s)
                
                void Startup (const StartupConfig& a_config,
                              FatalExceptionCallback a_fatal_exception_callback);
                void Run      (const float& a_polling_timeout = -1.0f);
                void Shutdown (int a_sig_no);
                
            public: // Inline Method(s) / Function(s)
                
                void                      Quit          ();
                
            protected: // Inline Method(s) / Function(s)
                
                const ev::Loggable::Data& loggable_data () const;
                ev::curl::HTTP&           HTTP          ();
                
            protected: // Pure Method(s) / Function(s)
                
                virtual void InnerStartup  (const ::cc::global::Process& a_process, const StartupConfig& a_startup_config, const Json::Value& a_config, SharedConfig& o_config) = 0;
                virtual void InnerShutdown () = 0;
                
            protected: // Threading Helper Methods(s) / Function(s)
                
                void PushJob               (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr);
                void ExecuteOnMainThread   (std::function<void()> a_callback, bool a_blocking);
                void ExecuteOnLooperThread (std::function<void()> a_callback, bool a_blocking);
                void OnFatalException      (const ev::Exception& a_exception);
                
            protected:
                
                void OnGlobalInitializationCompleted (const cc::global::Process& a_process, const cc::global::Directories& a_directories, const void* a_args,
                                                      cc::global::Logs& o_logs);
                
            private: // Method(s) / Function(s)
                
                void ConsumerLoop (const float& a_polling_timeout);

            }; // end of class 'Runner'
            
            inline void Runner::Quit ()
            {
                quit_ = true;
            }
            
            inline const ev::Loggable::Data& Runner::loggable_data () const
            {
                return *loggable_data_;
            }
            
            inline ev::curl::HTTP& Runner::HTTP ()
            {
                return *http_;
            }
            
        } // end of namespace 'beanstalkd'
        
    } // end of namespace 'loop'
    
} // end of namespace 'ev'

#endif // NRS_EV_LOOP_BEANSTALKD_RUNNER_H_
