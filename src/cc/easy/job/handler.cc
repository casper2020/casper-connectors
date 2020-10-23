/**
* @file easy.h
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

#include "cc/easy/job/handler.h"

#include "cc/optarg.h"
#include "cc/threading/worker.h"

#include "cc/easy/job/job.h"

#include "cc/debug/types.h" //  CC_DEBUG_ON

#ifdef __APPLE__
#pragma mark - cc::easy::job::JobInitializer
#endif

/**
 * @brief This method will be called when it's time to initialize this singleton.
 *
 * @param a_job A referece to the owner of this class.
 */
cc::easy::job::HandlerInitializer::HandlerInitializer (cc::easy::job::Handler& a_handler)
: ::cc::Initializer<cc::easy::job::Handler>(a_handler)
{
    instance_.factories_ = nullptr;
}

/**
 * @brief Destructor.
 */
cc::easy::job::HandlerInitializer::~HandlerInitializer ()
{
    instance_.factories_ = nullptr;
}

#ifdef __APPLE__
#pragma mark - cc::easy::job::Handler
#endif

/**
 * @brief Call when this job instance is about to startup.
 *
 * @param a_process        Process info.
 * @param a_startup_config Startup config.
 * @param a_job_config     Job specific config
 * @param o_config         Merged shared config.
 */
void cc::easy::job::Handler::InnerStartup  (const ::cc::global::Process& a_process, const ::ev::loop::beanstalkd::StartupConfig& a_startup_config, const Json::Value& a_job_config, const ::ev::loop::beanstalkd::SharedConfig& o_config,
                                            ev::loop::beanstalkd::Runner::Factory& o_factory)
{
    const pid_t       pid      = a_process.pid_;
    const std::string pname    = a_process.name_;
    const uint64_t    instance = static_cast<uint64_t>(a_startup_config.instance_);
    const std::string logs_dir = o_config.directories_.log_;
    
    o_factory = [this, a_job_config, pid, instance, logs_dir, pname] (const std::string& a_tube) -> ev::loop::beanstalkd::Job* {
        const auto it = factories_->find(a_tube);
        if ( factories_->end() != it ) {
            
            CC_JOB_LOG_ENABLE(a_tube, logs_dir + a_tube + "." + std::to_string(instance) + ".log");
            
            Json::Value tube_cfg = Json::Value::null;
            
            // ... grab <tube> specific config override ...
            if ( true == a_job_config.isMember("tubes") ) {
                // ... load <tube> config ...
                tube_cfg = a_job_config["tubes"][a_tube.c_str()];
                // ... if available ...
                if ( false == tube_cfg.isNull() && true == tube_cfg.isObject() ) {
                    // ... exclude members that cannot be overridden ...
                    for ( auto k : { "service_id" } ) {
                        if ( true == tube_cfg.isMember(k) ) {
                            tube_cfg.removeMember(k);
                        }
                    }
                }
            }

            // ... copy 'process' config ...
            Json::Value config = a_job_config[pname.c_str()];
            // ... merge with 'tube' config ...
            MergeJSONValue(config, tube_cfg);

            // ... create new tube instance and we're done ...
            return it->second(loggable_data(), {
                /* pid_               */ pid,
                /* instance_          */ instance,
                /* service_id_        */ config.get("service_id"   ,        "development").asString(),
                /* transient_         */ config.get("transient"    ,                false).asBool(),
                /* min_progress_      */ config.get("min_progress",                     3).asInt(),
                /* log_level_         */ config.get("log_level"    , CC_JOB_LOG_LEVEL_INF).asInt(),
                /* other_             */ config
            });

        }
        return nullptr;
    };
}

/**
 * @brief Call when this job instance is about to shutdown.
 */
void cc::easy::job::Handler::InnerShutdown ()
{
    /* empty */
}

/**
 * @brief Start a 'job' handler.
 *
 * @param a_arguments This job arguments.
 * @param a_factories Tube factories.
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 */
int cc::easy::job::Handler::Start (const cc::easy::job::Handler::Arguments& a_arguments,
                                   const cc::easy::job::Handler::Factories& a_factories,
                                   const float& a_polling_timeout)
{
    const auto clean_shutdown = [] () {
        
        cc::easy::job::Handler::GetInstance().Shutdown(SIGQUIT);
        cc::easy::job::Handler::Destroy();
        
    };
    
    const auto fatal_shutdown = [&clean_shutdown] (const cc::Exception& a_cc_exception, bool a_clean = true) {
        
        fprintf(stderr, "\n~~~\n\n%s\n~~~\n", a_cc_exception.what());
        fflush(stderr);
        if ( true == a_clean ) {
            clean_shutdown();
        }
        exit(-1);
    };
    
    try {

        cc::OptArg opt (a_arguments.name_.c_str(), a_arguments.version_.c_str(), a_arguments.rel_date_.c_str(), a_arguments.banner_.c_str(),
            {
                // NOTES: THIS OBJECTS WILL BE DELETE WHEN cc::OptArg::~OptArg IS CALLED
                new cc::OptArg::String(/* a_long */ "config" , /* a_short */ 'c', /* a_optional */ false             , /* a_tag */ "uri"    , /* a_help */ "configuration file"  ),
                new cc::OptArg::UInt64(/* a_long */ "index"  , /* a_short */ 'i', /* a_optional */ false             , /* a_tag */ "index"  , /* a_help */ "index"               ),
                new cc::OptArg::UInt64(/* a_long */ "cluster", /* a_short */ 'k', /* a_default  */ (const uint64_t) 0, /* a_tag */ "cluster", /* a_help */ "cluster number"      ),
                new cc::OptArg::Switch(/* a_long */ "help"   , /* a_short */ 'h', /* a_optional */ true              , /* a_tag */            /* a_help */ "show help"           ),
                new cc::OptArg::Switch(/* a_long */ "version", /* a_short */ 'v', /* a_optional */ true              , /* a_tag */            /* a_help */ "show version"        )
            #ifdef CC_DEBUG_ON
              , new cc::OptArg::String(/* a_long */ "debug"  , /* a_short */ 'd', /* a_optional */ true              , /* a_tag */ "token", /* a_help */ "enable a debug token")
            #endif
            }
        );

        // ... enable debug tokens ...
        #ifdef CC_DEBUG_ON
        opt.SetListener('d', [](const cc::OptArg::Opt* a_opt) {
            CC_DEBUG_LOG_ENABLE(dynamic_cast<const cc::OptArg::String*>(a_opt)->value());
        });
        #endif

        const int argp = opt.Parse(a_arguments.argc_, a_arguments.argv_);
        
        if ( true == opt.IsSet('h') ) {
            opt.ShowHelp();
            return 0;
        }
        
        if ( true == opt.IsSet('v') ) {
            opt.ShowVersion();
            return 0;
        }
        
        if ( 0 != argp ) {
            opt.ShowHelp(opt.error());
            return -1;
        }
        
        // ... set thread name ...
        ::cc::threading::Worker::SetName(a_arguments.name_);

        // ... startup ...
        cc::easy::job::Handler::GetInstance().Startup({
            /* abbr_           */ a_arguments.abbr_,
            /* name_           */ a_arguments.name_,
            /* version_        */ a_arguments.version_,
            /* rel_date_       */ a_arguments.rel_date_,
            /* info_           */ a_arguments.info_,
            /* banner_         */ a_arguments.banner_,
            /* instance_       */ static_cast<int>(opt.GetUInt64('i')->value()),
            /* cluster_        */ static_cast<int>(opt.GetUInt64('k')->value()),
            /* exec_path_      */ a_arguments.argv_[0],
            /* conf_file_uri_  */  opt.GetString('c')->value(),
        }, fatal_shutdown);
        
        // ... set this handler specific configs ...
        cc::easy::job::Handler::GetInstance().factories_ = &a_factories;

        // ... run ...
        cc::easy::job::Handler::GetInstance().Run(a_polling_timeout);

    } catch (const ::cc::Exception& a_cc_exception ) {
        fatal_shutdown(a_cc_exception, false);
    } catch (...) {
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (const ::cc::Exception& a_cc_exception) {
            fatal_shutdown(a_cc_exception, false);
        }
    }
    
    // ... done ...
    return 0;
}

/**
 * @brief Merge JSON Value.
 *
 * @param a_lhs Primary value.
 * @param a_rhs Override value.
 */
void cc::easy::job::Handler::MergeJSONValue (Json::Value& a_lhs, const Json::Value& a_rhs)
{
    if ( false == a_lhs.isObject() || false == a_rhs.isObject() ) {
        return;
    }
    for ( const auto& k : a_rhs.getMemberNames() ) {
        if ( true == a_lhs[k].isObject() && true == a_rhs[k].isObject() ) {
            MergeJSONValue(a_lhs[k], a_rhs[k]);
        } else {
            a_lhs[k] = a_rhs[k];
        }
    }
}