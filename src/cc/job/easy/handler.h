/**
 * @file handler.h
 *
 * Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_EASY_JOB_HANDLER_H_
#define NRS_CC_EASY_JOB_HANDLER_H_

#include "cc/singleton.h"

#include "ev/loop/beanstalkd/runner.h"

#include "cc/job/easy/job.h"

namespace cc
{

    namespace job
    {

        namespace easy
        {

            // ---- //
            class Handler;
            class HandlerInitializer final : public ::cc::Initializer<Handler>
            {
                
            public: // Constructor(s) / Destructor
                
                HandlerInitializer (Handler& a_handler);
                virtual ~HandlerInitializer ();
                
            };

            // ---- //
            class Handler final : public ev::loop::beanstalkd::Runner, public ::cc::Singleton<Handler, HandlerInitializer>
            {
                
                friend class HandlerInitializer;
                
            public: // Data Type(s)
                
                typedef struct {
                    const std::string  abbr_;
                    const std::string  name_;
                    const std::string  version_;
                    const std::string  rel_date_;
                    const std::string  info_;
                    const std::string  banner_;
                    const int          argc_;
                    const char** const argv_;
                } Arguments;
                
                typedef std::function<cc::job::easy::Job*(const ev::Loggable::Data&, const cc::job::easy::Job::Config&)> Factory;
                typedef std::map<std::string, Factory>       Factories;
                
            private: // Data
                
                const Factories* factories_;
                
            protected: // Inherited Pure Method(s) / Function(s)
                
                virtual void InnerStartup  (const StartupConfig& a_startup_config, const Json::Value& a_config, SharedConfig& o_config);
                virtual void InnerShutdown ();
                
            public: // Static Method(s) / Function(s)
                
                static int Start (const Arguments& a_config, const Factories& a_factories);
                
            }; // end of class 'Handler'

        } // end of namespace 'easy'

    } // end of namespace 'job'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_JOB_HANDLER_H_
