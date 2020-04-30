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

#include "cc/job/easy/handler.h"

#include "cc/optarg.h"
#include "cc/threading/worker.h"

#include "cc/job/easy/tube.h"

#ifdef __APPLE__
#pragma mark - cc::job::easy::JobInitializer
#endif

/**
 * @brief This method will be called when it's time to initialize this singleton.
 *
 * @param a_job A referece to the owner of this class.
 */
cc::job::easy::HandlerInitializer::HandlerInitializer (cc::job::easy::Handler& a_handler)
: ::cc::Initializer<cc::job::easy::Handler>(a_handler)
{
    instance_.factories_ = nullptr;
}

/**
 * @brief Destructor.
 */
cc::job::easy::HandlerInitializer::~HandlerInitializer ()
{
    instance_.factories_ = nullptr;
}

#ifdef __APPLE__
#pragma mark - cc::job::easy::Job
#endif

/**
 * @brief Call when this job instance is about to startup.
 *
 * @param a_startup_config Startup config.
 * @param a_job_config     Job specific config
 * @param o_config         Merged shared config.
 */
void cc::job::easy::Handler::InnerStartup  (const cc::job::easy::Handler::StartupConfig& /* a_startup_config */,
                                            const Json::Value& a_job_config, cc::job::easy::Handler::SharedConfig& o_config)
{
    o_config.factory_ = [this, &a_job_config] (const std::string& a_tube) -> ev::loop::beanstalkd::Job* {
        const auto it = factories_->find(a_tube);
        if ( factories_->end() != it ) {
            const Json::Value& config  = a_job_config[a_tube.c_str()];
            const Json::Value& options = a_job_config["options"];
            return new cc::job::easy::Tube(loggable_data(), "document-scanner",
                                           {
                                            /* service_id_        */ config.get("service_id"   , "development").asString(),
                                            /* transient_         */ config.get("transient"    ,         false).asBool(),
                                            /* min_progress_      */ options.get("min_progress",             3).asInt()
                                           },
                                           /* a_runnable */ it->second()
            );
        }
        return nullptr;
    };
}

/**
 * @brief Call when this job instance is about to shutdown.
 */
void cc::job::easy::Handler::InnerShutdown ()
{
    /* empty */
}

/**
 * @brief Start a 'job' handler.
 *
 * @param a_argumenta This job arguments.
 * @param a_factories Tube factories.
 */
int cc::job::easy::Handler::Start (const cc::job::easy::Handler::Arguments& a_arguments,
                                   const cc::job::easy::Handler::Factories& a_factories)
{
    const auto clean_shutdown = [] () {
        
        cc::job::easy::Handler::GetInstance().Shutdown(SIGQUIT);
        cc::job::easy::Handler::Destroy();
        
    };
    
    const auto fatal_shutdown = [&clean_shutdown] (const cc::Exception& a_cc_exception, bool a_clean = true) {
        fprintf(stderr, "%s\n", a_cc_exception.what());
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
                new cc::OptArg::String(/* a_long */ "--config" , /* a_short */ 'c', /* a_optional */ false , /* a_tag */ "uri"  , /* a_help */ "configuration file"),
                new cc::OptArg::UInt64(/* a_long */ "--index"  , /* a_short */ 'i', /* a_optional */ false , /* a_tag */ "index", /* a_help */ "index"             ),
                new cc::OptArg::Switch(/* a_long */ "--help"   , /* a_short */ 'h', /* a_optional */ true  , /* a_tag */          /* a_help */ "show help"         ),
                new cc::OptArg::Switch(/* a_long */ "--version", /* a_short */ 'v', /* a_optional */ true  , /* a_tag */          /* a_help */ "show version"      )
            }
        );
        
        const int argp = opt.Parse(a_arguments.argc_, a_arguments.argv_);
        if ( 0 != argp ) {
            opt.ShowHelp(opt.error());
            return -1;
        }
        
        if ( true == opt.IsSet('h') ) {
            opt.ShowHelp();
            return 0;
        }
        
        if ( true == opt.IsSet('v') ) {
            opt.ShowVersion();
            return 0;
        }
        
        // ... set thread name ...
        ::cc::threading::Worker::SetName(a_arguments.name_);

        // ... startup ...
        cc::job::easy::Handler::GetInstance().Startup({
            /* abbr_           */ a_arguments.abbr_,
            /* name_           */ a_arguments.name_,
            /* version_        */ a_arguments.version_,
            /* rel_date_       */ a_arguments.rel_date_,
            /* info_           */ a_arguments.info_,
            /* banner_         */ a_arguments.banner_,
            /* instance_       */ static_cast<int>(opt.GetUInt64('i')->value()),
            /* exec_path_      */ a_arguments.argv_[0],
            /* conf_file_uri_  */  opt.GetString('c')->value(),
        }, fatal_shutdown);
        

        // ... set this handler specific configs ...
        cc::job::easy::Handler::GetInstance().factories_ = &a_factories;

        // ... run ...
        cc::job::easy::Handler::GetInstance().Run();

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

