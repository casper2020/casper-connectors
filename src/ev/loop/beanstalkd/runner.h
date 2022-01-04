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

                typedef ev::loop::beanstalkd::Job::Factory                       Factory;
                typedef std::function<void(const ev::Exception& a_ev_exception)> FatalExceptionCallback;
                
                typedef std::function<void(const ::cc::global::Process&, const StartupConfig&, const Json::Value&, const SharedConfig&, Factory&)> InnerStartup;
                typedef std::function<void()>                                                                                                      InnerShutdown;
                
            private: // Data
                
                bool                     initialized_;
                bool                     shutting_down_;
                volatile bool            hard_abort_;
                volatile bool            soft_abort_;
                ev::loop::Bridge*        bridge_;
                std::thread*             consumer_thread_;
                osal::ConditionVariable* consumer_cv_;
                StartupConfig*           startup_config_;
                SharedConfig*            shared_config_;                
                ev::Loggable::Data*      loggable_data_;
                Factory                  factory_;

            private: // Helpers
                
                ev::curl::HTTP*          http_;
                
            private: // Callbacks
                
                ev::loop::beanstalkd::Runner::FatalExceptionCallback on_fatal_exception_;
                InnerStartup                                         inner_startup_;
                InnerShutdown                                        inner_shutdown_;
                
            private: // Threading
                
                std::mutex                    looper_mutex_;
                ev::loop::beanstalkd::Looper* looper_ptr_;
                std::atomic<bool>             running_;
                std::atomic<int>              rv_;

            public: // Constructor(s) / Destructor
                
                Runner ();
                virtual ~Runner();
                
            public: // Method(s) / Function(s)
                
                void Startup (const StartupConfig& a_config,
                              InnerStartup a_inner_startup, InnerShutdown a_inner_shutdown,
                              FatalExceptionCallback a_fatal_exception_callback);
                int  Run      (const float& a_polling_timeout = -1.0f, const bool a_at_main_thread = false);
                void Shutdown (int a_sig_no);
                
            public: // Inline Method(s) / Function(s)
                
                void                      Quit          (const bool a_soft);
                
            public: // Inline Method(s) / Function(s)
                
                const ev::Loggable::Data& loggable_data () const;

            protected: // Inline Method(s) / Function(s)
                
                ev::curl::HTTP&           HTTP          ();
                                
            protected: // Threading Helper Methods(s) / Function(s)
                
                void PushJob               (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr);
                void ExecuteOnMainThread   (std::function<void()> a_callback, bool a_blocking);
                void ScheduleOnMainThread  (std::function<void()> a_callback, const size_t a_deferred);
                
                void ScheduleCallbackOnLooperThread (const std::string& a_id, ev::loop::beanstalkd::Looper::IdleCallback a_callback,
                                                     const size_t a_deferred, const bool a_recurrent);
                
                void TryCancelCallbackOnLooperThread (const std::string& a_id);
                
                void OnFatalException              (const ev::Exception& a_exception);
                
            protected:
                
                void OnGlobalInitializationCompleted (const cc::global::Process& a_process, const cc::global::Directories& a_directories, const void* a_args,
                                                      cc::global::Logs& o_logs);
                
            private: // Method(s) / Function(s)
                
                void ConsumerLoop (const float& a_polling_timeout);

            }; // end of class 'Runner'
            
            inline void Runner::Quit (const bool a_soft)
            {
                if ( true == a_soft ) {
                    soft_abort_ = true;
                } else {
                    hard_abort_ = true;
                }
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
