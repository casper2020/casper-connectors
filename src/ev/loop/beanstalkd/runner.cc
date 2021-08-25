/**
 * @file runner
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

#include "ev/loop/beanstalkd/runner.h"

#ifdef CASPER_REQUIRE_GOOGLE_V8
    #include "cc/v8/singleton.h"
#endif

#include "ev/redis/device.h"
#include "ev/redis/subscriptions/manager.h"

#include "ev/postgresql/device.h"
#include "ev/curl/device.h"

#include "osal/osalite.h"

#include "cc/debug/types.h"
#include "cc/utc_time.h"
#include "cc/global/initializer.h"

#include "unicode/locid.h"  // locale
#include "unicode/utypes.h" // u_init
#include "unicode/uclean.h" // u_cleanup

#include "cc/threading/worker.h"

#include <iterator> // std::advance
#include <fstream>  // std::ifstream

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 */
ev::loop::beanstalkd::Runner::Runner ()
{
    initialized_              = false;
    shutting_down_            = false;
    quit_                     = false;
    bridge_                   = nullptr;
    consumer_thread_          = nullptr;
    consumer_cv_              = nullptr;
    startup_config_           = nullptr;
    shared_config_            = new ev::loop::beanstalkd::SharedConfig({
        /* ip_addr_ */ "",
        /* directories_ */ {
            /* log_    */ "",
            /* run_    */ "",
            /* lock_   */ "",
            /* shared_ */ "",
            /* output_ */ "",
        },
        /* log_tokens_ */ {},
        /* redis_ */
        {
            /* host_     */ "127.0.0.1",
            /* port_     */ 6379,
            /* database_ */ -1,
            /* limits_   */
            {
                /* max_conn_per_worker_  */  2,
                /* max_queries_per_conn_ */ -1,
                /* min_queries_per_conn_ */ -1
            }
        },
        /* postgres_ */
        {
            /* conn_str_             */ "",
            /* statement_timeout_    */ 300,
            /* post_connect_queries_ */ nullptr,
            /* limits_ */
            {
                /* max_conn_per_worker_  */  2,
                /* max_queries_per_conn_ */ -1,
                /* min_queries_per_conn_ */ -1
            }
        },
        /* beanstalk_ */
        {
            /* host_              */ "127.0.0.1",
            /* port_              */ 11300,
            /* timeout_           */ 0.0,
            /* abort_polling_     */ 3,
            /* max_attempts_      */ std::numeric_limits<uint64_t>::max(),
            /* tubes_             */ {} ,
            /* sessionless_tubes_ */ {},
            /* action_tubes_      */ {}
        },
        /* device_limits_ */ {}
    });
    loggable_data_ = nullptr;
    factory_       = nullptr;
    http_          = nullptr;
    looper_ptr_    = nullptr;
    running_       = false;
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Runner::~Runner ()
{
    if ( nullptr != bridge_ ) {
        delete bridge_;
    }
    if ( nullptr != consumer_thread_ ) {
        delete consumer_thread_;
    }
    if ( nullptr != consumer_cv_ ) {
        delete consumer_cv_;
    }
    if ( nullptr != startup_config_ ) {
        delete startup_config_;
    }
    if ( nullptr != shared_config_ ) {
        if ( nullptr != shared_config_->postgres_.post_connect_queries_ ) {
            delete shared_config_->postgres_.post_connect_queries_;
        }
        delete shared_config_;
    }
    factory_ = nullptr;
    if ( nullptr != loggable_data_ ) {
        delete loggable_data_;
    }
    if ( nullptr != http_ ) {
        delete http_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief One-shot initializer.
 *
 * @param a_config
 * @param a_fatal_exception_callback
 */
void ev::loop::beanstalkd::Runner::Startup (const ev::loop::beanstalkd::StartupConfig& a_config,
                                            ev::loop::beanstalkd::Runner::InnerStartup a_inner_startup, ev::loop::beanstalkd::Runner::InnerShutdown a_inner_shutdown,
                                            ev::loop::beanstalkd::Runner::FatalExceptionCallback a_fatal_exception_callback)
{
    inner_startup_  = a_inner_startup;
    inner_shutdown_ = a_inner_shutdown;
    
    const pid_t process_pid = getpid();

    CC_IF_DEBUG_DECLARE_VAR(std::set<std::string>, debug_tokens);
    CC_IF_DEBUG(
        debug_tokens.insert("exceptions");
    );
    
    // (<name>.<pid>)[.<cluster>].<instance>(.<ext>)
    std::string log_fn_component;
    if ( 0 != a_config.cluster_ ) {
        log_fn_component = '.' + std::to_string(a_config.cluster_) + '.' + std::to_string(a_config.instance_);
    } else {
        log_fn_component = '.' + std::to_string(a_config.instance_);
    }

    //
    // ... initialize casper-connectors // third party libraries ...
    //
    #ifdef CASPER_REQUIRE_GOOGLE_V8
        const bool v8_required = true;
    #else
        const bool v8_required = false;
    #endif
        
    ::cc::global::Initializer::GetInstance().WarmUp(
        /* a_process */
        {
            /* name_       */ a_config.name_,
            /* alt_name_   */ "",
            /* abbr_       */ a_config.abbr_,
            /* version_    */ a_config.version_,
            /* rel_date_   */ a_config.rel_date_,
            /* rel_branch_ */ a_config.rel_branch_,
            /* rel_hash_   */ a_config.rel_hash_,
            /* info_       */ a_config.info_,
            /* banner_     */ a_config.banner_,
            /* pid_        */ process_pid,
            /* standalone_ */ true,
            /* is_master_  */ true
        },
        /* a_directories */
        nullptr, /* using defaults */
        /* a_logs */
        {},
        /* a_v8 */
        {
            /* required_            */ v8_required,
            /* runs_on_main_thread_ */ false
        },
        /* a_next_step */
        {
            /* function_ */ std::bind(&ev::loop::beanstalkd::Runner::OnGlobalInitializationCompleted, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
            /* args_     */ (void*)(&a_config)
        },
        /* a_present */ [this, &a_config] (std::vector<::cc::global::Initializer::Present>& o_values) {
            // ... config file ...
            {
                o_values.push_back({
                    /* title_   */ "CONFIG",
                    /* values_ */ { { "URI" , a_config.conf_file_uri_ } }
                });
                auto& config = o_values.back();
                if ( -0 != a_config.cluster_ ) {
                    config.values_["CLUSTER"] = std::to_string(a_config.cluster_);
                }
                config.values_["INSTANCE"] = std::to_string(a_config.instance_);
            }
            // ... tubes ...
            {
                o_values.push_back({
                    /* title_   */ "TUBES",
                    /* values_ */ {}
                });
                auto& tubes = o_values.back();
                for ( auto it : shared_config_->beanstalk_.tubes_ ) {
                    tubes.values_["tubes[" + std::to_string(tubes.values_.size()) + "]"] = it;
                }
            }
        },
        /* a_debug_tokens */
        CC_IF_DEBUG_ELSE(&debug_tokens, nullptr),
        /* a_use_local_dirs */ true,
        /* a_log_fn_component */ log_fn_component
    );
    
    on_fatal_exception_ = a_fatal_exception_callback;
    
    ::cc::global::Initializer::GetInstance().Startup(
        /* a_signals */
        {
            /* register_ */ { SIGUSR1, SIGQUIT, SIGTERM, SIGTTIN },
            /* on_signal_ */
            [this](const int a_sig_no) { // unhandled signals callback
                // ... is a 'shutdown' signal?
                switch(a_sig_no) {
                    case SIGQUIT:
                    case SIGTERM:
                    {
                        Quit();
                    }
                    return true;
                default:
                    return false;
                }
            }
        },
        /* a_callbacks */
        {
            /* call_on_main_thread_    */
            [this] (std::function<void()> a_callback) {
                ExecuteOnMainThread(a_callback, /* a_blocking */ false);
            }
        }
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Call this function to set nginx-broker process.
 *
 * @param a_process     Previously set process info.
 * @param a_directories Previously set directories info
 * @param a_arg         \link StartupConfig \link
 * @param o_logs        Post initialization additional logs to enable.
 */
void ev::loop::beanstalkd::Runner::OnGlobalInitializationCompleted (const ::cc::global::Process& a_process,
                                                                    const ::cc::global::Directories& a_directories,
                                                                    const void* a_args,
                                                                    ::cc::global::Logs& o_logs)
{
    //
    // Copy Startup Config
    //
    startup_config_ = new ev::loop::beanstalkd::StartupConfig(*(const ev::loop::beanstalkd::StartupConfig*)a_args);

    // (<name>.<pid>)[.<cluster>].<instance>(.<ext>)
    std::string fn_ci_component;
    if ( 0 != startup_config_->cluster_ ) {
        fn_ci_component = '.' + std::to_string(startup_config_->cluster_) + '.' + std::to_string(startup_config_->instance_);
    } else {
        fn_ci_component = '.' + std::to_string(startup_config_->instance_);
    }

    //
    // Work directories:
    //
    shared_config_->directories_ = {
        /* log_    */ a_directories.log_,
        /* run_    */ a_directories.run_,
        /* lock_   */ a_directories.lock_,
        /* shared_ */ a_directories.share_,
        /* output_ */ a_directories.tmp_
    };

    //
    // Load config:
    //

    std::ifstream f_stream(startup_config_->conf_file_uri_);
    if ( false == f_stream.is_open() ) {
        throw ev::Exception("Unable to open configuration file '%s'!", startup_config_->conf_file_uri_.c_str());
    }
    
    std::string data((std::istreambuf_iterator<char>(f_stream)), std::istreambuf_iterator<char>());
    
    Json::Reader json_reader;
    Json::Value read_config = Json::Value::null;
    if ( false == json_reader.parse(data, read_config) ) {
        f_stream.close();
        throw ev::Exception("An error occurred while loading configuration: JSON parsing error - %s\n", json_reader.getFormattedErrorMessages().c_str());
    } else if ( Json::ValueType::objectValue != read_config.type() ) {
        f_stream.close();
        throw ev::Exception("An error occurred while loading configuration: unexpected JSON object - object as top object is expected!");
    } else {
        f_stream.close();
    }    
    
    try {

        /* postgresql */
        const Json::Value postgres = read_config.get("postgres", Json::Value::null);
        if ( true == postgres.isNull() || Json::ValueType::objectValue != postgres.type() ) {
            throw ev::Exception("An error occurred while loading configuration: missing or invalid 'postgres' object!");
        }
        const Json::Value conn_str = postgres.get("conn_str", Json::Value::null);
        if ( true == conn_str.isNull() ) {
            throw ev::Exception("An error occurred while loading configuration - missing or invalid PostgreSQL connection string!");
        }
        
        shared_config_->postgres_.conn_str_                     = conn_str.asString();
        shared_config_->postgres_.statement_timeout_            = postgres.get("statement_timeout", shared_config_->postgres_.statement_timeout_).asInt();
        shared_config_->postgres_.limits_.max_conn_per_worker_  = static_cast<size_t>(postgres.get("max_conn_per_worker"  , static_cast<int>(shared_config_->postgres_.limits_.max_conn_per_worker_ )).asInt());
        shared_config_->postgres_.limits_.max_queries_per_conn_ = static_cast<ssize_t>(postgres.get("max_queries_per_conn", static_cast<int>(shared_config_->postgres_.limits_.max_queries_per_conn_)).asInt());
        shared_config_->postgres_.limits_.min_queries_per_conn_ = static_cast<ssize_t>(postgres.get("min_queries_per_conn", static_cast<int>(shared_config_->postgres_.limits_.min_queries_per_conn_)).asInt());
        
        const Json::Value post_connect_queries = postgres.get("post_connect_queries", Json::Value::null);
        if ( false == post_connect_queries.isNull() ) {
            if ( false == post_connect_queries.isArray() ) {
                throw ev::Exception("An error occurred while loading configuration - invalid PostgreSQL post connect object ( array of strings is expected )!");
            } else {
                for ( Json::ArrayIndex idx = 0 ; idx < post_connect_queries.size() ; ++idx ) {
                    if ( false == post_connect_queries[idx].isString() || 0 == post_connect_queries[idx].asString().length() ) {
                        throw ev::Exception("An error occurred while loading configuration - invalid PostgreSQL post connect object at index %d ( strings is expected )!", idx);
                    }
                }
            }
            shared_config_->postgres_.post_connect_queries_ = new Json::Value(post_connect_queries);
        }
        
        /* beanstalkd */
        const Json::Value beanstalkd = read_config.get("beanstalkd", Json::Value::null);
        if ( false == beanstalkd.isNull() ) {
            shared_config_->beanstalk_.host_          = beanstalkd.get("host"         , shared_config_->beanstalk_.host_         ).asString();
            shared_config_->beanstalk_.port_          = beanstalkd.get("port"         , shared_config_->beanstalk_.port_         ).asInt();
            shared_config_->beanstalk_.timeout_       = beanstalkd.get("timeout"      , shared_config_->beanstalk_.timeout_      ).asFloat();
            shared_config_->beanstalk_.abort_polling_ = beanstalkd.get("abort_polling", shared_config_->beanstalk_.abort_polling_).asFloat();
            shared_config_->beanstalk_.max_attempts_  = beanstalkd.get("max_attempts" , shared_config_->beanstalk_.max_attempts_).asUInt64();
            shared_config_->beanstalk_.tubes_.clear();
            const Json::Value& tubes = beanstalkd["tubes"];
            if ( false == tubes.isArray() ) {
                const Json::Value& tube = beanstalkd["tube"];
                if ( false == tube.isString() ) {
                    throw ev::Exception("An error occurred while loading configuration - invalid tubes type!");
                }
                shared_config_->beanstalk_.tubes_.insert(tube.asString());
            }
            if ( tubes.size() > 0 ) {
                for ( Json::ArrayIndex idx = 0 ; idx < tubes.size() ; ++idx ) {
                    shared_config_->beanstalk_.tubes_.insert(tubes[idx].asString());
                }
            }
        }
        
        /* redis */
        const Json::Value redis = read_config.get("redis", Json::Value::null);
        if ( false == redis.isNull() ) {
            shared_config_->redis_.host_     = redis.get("host"    , shared_config_->redis_.host_     ).asString();
            shared_config_->redis_.port_     = redis.get("port"    , shared_config_->redis_.port_     ).asInt();
            shared_config_->redis_.database_ = redis.get("database", shared_config_->redis_.database_ ).asInt();
            shared_config_->redis_.limits_.max_conn_per_worker_  = static_cast<size_t>(redis.get("max_conn_per_worker"  , static_cast<int>(shared_config_->redis_.limits_.max_conn_per_worker_ )).asInt());
            shared_config_->redis_.limits_.max_queries_per_conn_ = static_cast<ssize_t>(redis.get("max_queries_per_conn", static_cast<int>(shared_config_->redis_.limits_.max_queries_per_conn_)).asInt());
            shared_config_->redis_.limits_.min_queries_per_conn_ = static_cast<ssize_t>(redis.get("min_queries_per_conn", static_cast<int>(shared_config_->redis_.limits_.min_queries_per_conn_)).asInt());
        }
        
        /* process specific config */
        const Json::Value&  p_cfg = read_config[a_process.name_];
        if ( false == p_cfg.isNull() ) {
            /* host / ip address */
            if ( true == p_cfg.isMember("host") && true == p_cfg["host"].isString() ) {
                shared_config_->ip_addr_ = p_cfg["host"].asString();
            } else {
                shared_config_->ip_addr_ = "127.0.0.1";
            }
            /* logs */
            const Json::Value logs = p_cfg.get("logs", Json::Value::null);
            if ( false == logs.isNull() ) {
                shared_config_->directories_.log_ = logs.get("directory", shared_config_->directories_.log_).asString();
                if ( shared_config_->directories_.log_.length() > 0 ) {
                    const osal::Dir::Status mkdir_status = osal::Dir::CreateDir(shared_config_->directories_.log_.c_str());
                    if ( osal::Dir::EStatusOk != mkdir_status ) {
                        throw ev::Exception("An error occurred while creating logs directory: %s!", shared_config_->directories_.log_.c_str());
                    }
                    // ... normalize ...
                    if ( shared_config_->directories_.log_[shared_config_->directories_.log_.length() - 1] != '/' ) {
                        shared_config_->directories_.log_ += '/';
                    }

                    const Json::Value tokens = logs.get("tokens", Json::Value::null);
                    if ( false == tokens.isNull() ) {
                        if ( false == tokens.isArray() ) {
                            throw ev::Exception("An error occurred while creating preparing log tokens: expecting an JSON array of strings!");
                        }
                        for ( Json::ArrayIndex idx = 0 ; idx < tokens.size() ; ++idx ) {
                            const std::string token = tokens[idx].asString();
                            if ( 0 == token.length() ) {
                                continue;
                            }
                            shared_config_->log_tokens_[token] = shared_config_->directories_.log_ + token + fn_ci_component + ".log";
                        }
                    }
                }
            }
        }
        
        inner_startup_(a_process, *startup_config_, read_config, *shared_config_, factory_);

    } catch (const Json::Exception& a_json_exception) {
        throw ev::Exception("An error occurred while loading configuration: JSON exception %s!", a_json_exception.what());
    }
        
    // ... at macOS and if debug mode ...
#if defined(__APPLE__) && !defined(NDEBUG) && ( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
    // ... delete all log files ...
    osal::File::Delete(shared_config_->directories_.run_.c_str(), "*.pid", nullptr);
    osal::File::Delete(shared_config_->directories_.run_.c_str(), "ev-*.socket", nullptr);
#else // linux
    // ... log or release rotate should take care of this ...
#endif
        
    //
    // Write pid file for SYSTEMD:
    //
    const std::string pid_file = shared_config_->directories_.run_ + std::string( fn_ci_component.c_str() + 1 ) + ".pid";
    FILE* pid_fd = fopen(pid_file.c_str(), "w");
    if ( nullptr == pid_fd ) {
        throw ev::Exception("Unable to open file '%s' to write pid: nullptr", pid_file.c_str());
    } else {
        const int bytes_written = fprintf(pid_fd, "%d", a_process.pid_);
        if ( bytes_written <= 0 ) {
            fclose(pid_fd);
            throw ev::Exception("Unable to pid write to file '%s'", pid_file.c_str());
        }
    }
    fflush(pid_fd);
    fclose(pid_fd);
        
    // .. set loggable data ...
    loggable_data_ = new ::ev::Loggable::Data(/* owner_ptr_ */ this,
                                              /* ip_addr_   */ shared_config_->ip_addr_,
                                              /* module_    */ startup_config_->info_,
                                              /* tag_       */ ( 0 != startup_config_->cluster_ ? "-k " + std::to_string(startup_config_->cluster_) + "; " : "" ) + "-i " + std::to_string(startup_config_->instance_)
    );
    
    // ... set a http client ...
    http_ = new ev::curl::HTTP();
    
    //
    // ... set PERMANENT DEBUG log tokens ...
    //
#if 0
    CC_IF_DEBUG(
      // ... v0 ...
      for ( auto token : { "ev_bridge", "ev_hub", "ev_scheduler", "ev_bridge_handler" } ) {
          o_logs.push_back({ /* token_ */ token ,/* uri_ */ shared_config_->directories_.log_ + +"debug." + token + fn_ci_component + ".log", /* conditional_ */ false, /* enabled_ */ true, /* version_ */ 0 });
      }
    );
#endif

    //
    // ... set PERMANENT log tokens  ...
    //
    
    // ... v1 ...
    for ( auto token : { "libpq-connections", "libpq" } ) {
        o_logs.push_back({ /* token_ */ token ,/* uri_ */ shared_config_->directories_.log_ + token + fn_ci_component + ".log", /* conditional_ */ false, /* enabled_ */ true, /* version_ */ 1 });
    }
    // ... v2 ...
    for ( auto token : { "signals", "queue" } ) {
        o_logs.push_back({ /* token_ */ token ,/* uri_ */ shared_config_->directories_.log_ + token + fn_ci_component + ".log", /* conditional_ */ false, /* enabled_ */ true, /* version_ */ 2 });
    }
    
    //
    // SOCKETS:
    //
    std::stringstream ss;
    
    // ... prepare socket file names ...
    ss.str("");
    ss << shared_config_->directories_.run_ << "ev-scheduler" << fn_ci_component << '.' << a_process.pid_ << ".socket";
    const std::string scheduler_socket_fn = ss.str();
    
    ss.str("");
    ss << shared_config_->directories_.run_ << "ev-shared-handler" << fn_ci_component << '.' << a_process.pid_ << ".socket";
    const std::string  shared_handler_socket_fn = ss.str();
    
    //
    // BRIDGE
    //
    bridge_ = new ev::loop::Bridge();
    (void)bridge_->Start(startup_config_->abbr_, shared_handler_socket_fn, std::bind(&ev::loop::beanstalkd::Runner::OnFatalException, this, std::placeholders::_1));
    
    //
    // SCHEDULER
    //
    osal::ConditionVariable scheduler_cv;
    // ... then initialize scheduler ...
    ::ev::scheduler::Scheduler::GetInstance().Start(startup_config_->abbr_,
                                                    scheduler_socket_fn,
                                                    *bridge_,
                                                    [&scheduler_cv]() {
                                                        scheduler_cv.Wake();
                                                    },
                                                    [this] (const ::ev::Object* a_object) -> ::ev::Device* {
                                                        
                                                        switch (a_object->target_) {
                                                            case ::ev::Object::Target::Redis:
                                                                return new ::ev::redis::Device(/* a_loggable_data */
                                                                                               *loggable_data_,
                                                                                               /* a_client_name */
                                                                                               startup_config_->name_,
                                                                                               /* a_ip_address */
                                                                                               shared_config_->redis_.host_.c_str(),
                                                                                               /* a_port */
                                                                                               shared_config_->redis_.port_,
                                                                                               /* a_database_index */
                                                                                               shared_config_->redis_.database_
                                                                );
                                                            case ::ev::Object::Target::PostgreSQL:
                                                                return new ::ev::postgresql::Device(/* a_loggable_data */
                                                                                                    *loggable_data_,
                                                                                                    /* a_conn_str */
                                                                                                    shared_config_->postgres_.conn_str_.c_str(),
                                                                                                    /* a_statement_timeout */
                                                                                                    shared_config_->postgres_.statement_timeout_,
                                                                                                    /* a_post_connect_queries */
                                                                                                    nullptr != shared_config_->postgres_.post_connect_queries_ ? *shared_config_->postgres_.post_connect_queries_ : Json::Value::null,
                                                                                                    /* a_max_queries_per_conn */
                                                                                                    ev::DeviceLimits::s_rnd_queries_per_conn_(shared_config_->postgres_.limits_)
                                                                );
                                                            case ::ev::Object::Target::CURL:
                                                                return new ::ev::curl::Device(/* a_loggable_data */
                                                                                              *loggable_data_
                                                                );
                                                            default:
                                                                return nullptr;
                                                        }
                                                        
                                                    },
                                                    [this](::ev::Object::Target a_target) -> size_t {
                                                        
                                                        const auto it = shared_config_->device_limits_.find(a_target);
                                                        if ( shared_config_->device_limits_.end() != it ) {
                                                            return it->second.max_conn_per_worker_;
                                                        }
                                                        return 2;
                                                    }
    );
    
    scheduler_cv.Wait();
    
    //
    // REDIS SUBSCRIPTIONS
    //
    // ... finally, initialize 'redis' subscriptions ...
    ::ev::redis::subscriptions::Manager::GetInstance().Startup(/* a_loggable_data */
                                                               *loggable_data_,
                                                               /* a_bridge */
                                                               bridge_,
                                                               /* a_channels */
                                                               {},
                                                               /* a_patterns */
                                                               {},
                                                               /* a_timeout_config */
                                                               {
                                                                   /* callback_ */
                                                                   nullptr,
                                                                   /* sigabort_file_uri_ */
                                                                   // TODO
                                                                   "", /* "/tmp/cores/casper-print-queue-sigabort_on_redis_subscribe_timeout" */
                                                               }
    );
    
    // ... mark as initialized ...
    initialized_ = true;
}

/**
 * @brief Run 'bridge' loop.
 *
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 * @param a_at_main_thread True if this will run at main thread.
 */
void ev::loop::beanstalkd::Runner::Run (const float& a_polling_timeout, const bool a_at_main_thread)
{
    consumer_cv_     = new osal::ConditionVariable();
    consumer_thread_ = new std::thread(&ev::loop::beanstalkd::Runner::ConsumerLoop, this, a_polling_timeout);
    consumer_thread_->detach();
    
    consumer_cv_->Wait();
    
    bridge_->Loop(a_at_main_thread);
    
    if ( nullptr != consumer_cv_ ) {
        delete consumer_cv_;
        consumer_cv_ = nullptr;
    }
    
    if ( nullptr != consumer_thread_ ) {
        delete consumer_thread_;
        consumer_thread_ = nullptr;
    }
}

/**
 * @brief Stop running loop.
 *
 * @param a_sig_no
 */
void ev::loop::beanstalkd::Runner::Shutdown (int a_sig_no)
{

    // ... shutdown already scheduled?
    if ( true == shutting_down_ ) {
        // ... yes ...
        return;
    }

    // ... no, but will be now ...
    shutting_down_ = true;
    
    // ... shutdown scheduled ...
    ::ev::scheduler::Scheduler::GetInstance().Stop(/* a_finalization_callback */ nullptr, /* a_sig_no */ -1);

    // ... clean up
    ::osal::ConditionVariable cleanup_cv;
    const auto cleanup = ([this, a_sig_no, &cleanup_cv](){
        
        // ... then shutdown 'redis' subscriptions ...
        ::ev::redis::subscriptions::Manager::GetInstance().Shutdown();
        
        // ... bridge too ...
        if ( nullptr != bridge_ ) {
            bridge_->Stop(a_sig_no);
            delete bridge_;
            bridge_ = nullptr;
        }
        
        // ... consumer thread can now be release ...
        if ( nullptr != consumer_thread_ ) {
            delete consumer_thread_;
            consumer_thread_ = nullptr;
        }
        
        if ( nullptr != consumer_cv_ ) {
            delete consumer_cv_;
            consumer_cv_ = nullptr;
        }

        inner_shutdown_();

        if ( nullptr != loggable_data_ ) {
            delete loggable_data_;
            loggable_data_ = nullptr;
        }
        if ( nullptr != http_ ) {
            delete http_;
            http_ = nullptr;
        }

        // ... reset initialized flag ...
        initialized_ = false;
        quit_        = false;
        
        // ... done ...
        cleanup_cv.Wake();
        
    });
    
    if ( true == bridge_->IsRunning() ) {
        ExecuteOnMainThread(cleanup, /* a_blocking */ false);
    } else {
        cleanup();
    }
    cleanup_cv.Wait();
        
    // ... casper-connectors // third party libraries cleanup ...
    ::cc::global::Initializer::GetInstance().Shutdown(/* a_for_cleanup_only */ false);
}

#ifdef __APPLE__
#pragma mark -
#pragma mark Threading
#endif

/**
 * @brief Push a 'beanstalkd' job queue.
 *
 * @param a_tube
 * @param a_payload
 * @param a_ttr
 */
void ev::loop::beanstalkd::Runner::PushJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)
{
    try {
        ::ev::beanstalk::Producer producer(shared_config_->beanstalk_, a_tube.c_str());
    
        const int64_t status = producer.Put(a_payload, /* a_priority = 0 */ 0, /* a_delay = 0 */ 0, a_ttr);
        if ( status < 0 ) {
            throw ::ev::Exception("Beanstalk producer returned with error code " INT64_FMT " - %s!",
                                  status, producer.ErrorCodeToString(status)
            );
        }
    } catch (const ::Beanstalk::ConnectException& a_btc_exception) {
        throw ::ev::Exception("%s", a_btc_exception.what());
    }
}

/**
 * @brief Execute a callback on main thread.
 *
 * @param a_callback
 * @param a_blocking
 */
void ev::loop::beanstalkd::Runner::ExecuteOnMainThread (std::function<void()> a_callback, bool a_blocking)
{
    if ( true == a_blocking ) {
        osal::ConditionVariable cv;
        bridge_->CallOnMainThread([&a_callback, &cv] {
            try {
                a_callback();
            } catch (...) {
                // ...
            }
            cv.Wake();
        });
        cv.Wait();
    } else {
        bridge_->CallOnMainThread(a_callback);
    }
}

/**
 * @brief Execute a callback on main thread.
 *
 * @param a_callback
 * @param a_blocking
 */
void ev::loop::beanstalkd::Runner::ScheduleOnMainThread (std::function<void()> a_callback, const size_t a_deferred)
{
    bridge_->CallOnMainThread(a_callback, static_cast<int64_t>(a_deferred));
}

/**
 * @brief Schedule a callback on 'the other' thread.
 *
 * @param a_id
 * @param a_callback
 * @param a_deferred
 * @param a_recurrent
 */
void ev::loop::beanstalkd::Runner::ScheduleCallbackOnLooperThread (const std::string& a_id, ev::loop::beanstalkd::Looper::IdleCallback a_callback,
                                                                   const size_t a_deferred, const bool a_recurrent)
{
    std::lock_guard<std::mutex> lock(looper_mutex_);
    if ( false == running_ || nullptr == looper_ptr_ ) {
        throw ev::Exception("Illegal call to '%s' - looper not ready!", __PRETTY_FUNCTION__);
    }
    looper_ptr_->AppendCallback(a_id, a_callback, a_deferred, a_recurrent);
}

/**
 * @brief Try to cancel a previously scheduled callback on 'the other' thread.
 *
 * @param a_id
 */
void ev::loop::beanstalkd::Runner::TryCancelCallbackOnLooperThread (const std::string& a_id)
{
    std::lock_guard<std::mutex> lock(looper_mutex_);
    if ( false == running_ || nullptr == looper_ptr_ ) {
        throw ev::Exception("Illegal call to '%s' - looper not ready!", __PRETTY_FUNCTION__);
    }
    looper_ptr_->RemoveCallback(a_id);
}

/**
 * @brief Report a faltal exception.
 *
 * @param a_exception
 */
void ev::loop::beanstalkd::Runner::OnFatalException (const ev::Exception& a_exception)
{
    ExecuteOnMainThread([this, a_exception]{
        on_fatal_exception_(a_exception);
    }, /* a_blocking */ ( false == running_ ));
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Print queue consumer loop.
 *
 * @param a_polling_timeout Consumer's loop polling timeout in millseconds, if < 0 will use defaults.
 */
void ev::loop::beanstalkd::Runner::ConsumerLoop (const float& a_polling_timeout)
{
    ev::Exception* exception = nullptr;
        
    
    const ev::loop::beanstalkd::Job::MessagePumpCallbacks callbacks = {
        /* on_fatal_exception_                       */ std::bind(&ev::loop::beanstalkd::Runner::OnFatalException,
                                                                  this, std::placeholders::_1),
        /* on_main_thread_                           */ std::bind(&ev::loop::beanstalkd::Runner::ExecuteOnMainThread,
                                                                  this, std::placeholders::_1, std::placeholders::_2),
        /* schedule_on_main_thread_                  */ std::bind(&ev::loop::beanstalkd::Runner::ScheduleOnMainThread,
                                                                  this, std::placeholders::_1, std::placeholders::_2),
        /* schedule_callback_on_the_looper_thread_   */ std::bind(&ev::loop::beanstalkd::Runner::ScheduleCallbackOnLooperThread,
                                                                  this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
        /* try_cancel_callback_on_the_looper_thread_ */ std::bind(&ev::loop::beanstalkd::Runner::TryCancelCallbackOnLooperThread,
                                                                  this, std::placeholders::_1),
        /* on_push_job_                              */ std::bind(&ev::loop::beanstalkd::Runner::PushJob,
                                                                  this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
    };
    
    try {
        
        ::cc::threading::Worker::SetName(startup_config_->abbr_ + "::Runner");
        ::cc::threading::Worker::BlockSignals({SIGUSR1, SIGTTIN, SIGTERM, SIGQUIT});

        // ... initialize v8 ...
        #ifdef CASPER_REQUIRE_GOOGLE_V8
            ::cc::v8::Singleton::GetInstance().Initialize();
        #endif
        
        consumer_cv_->Wake();
        
        running_ = true;
        
        looper_mutex_.lock();
        looper_ptr_ = new ev::loop::beanstalkd::Looper(*loggable_data_, factory_, callbacks);
        looper_mutex_.unlock();
        
        looper_ptr_->SetPollingTimeout(a_polling_timeout);
        looper_ptr_->Run(*shared_config_, quit_);
        
        running_ = false;
        
    } catch (const Beanstalk::ConnectException& a_beanstalk_exception) {
        exception = new ::ev::Exception("An error occurred while connecting to Beanstalkd:\n%s\n", a_beanstalk_exception.what());
    } catch (const osal::Exception& a_osal_exception) {
        exception = new ::ev::Exception("An error occurred during startup:\n%s\n", a_osal_exception.Message());
    } catch (const ev::Exception& a_ev_exception) {
        exception = new ::ev::Exception("An error occurred during startup:\n%s\n", a_ev_exception.what());
    } catch (const std::bad_alloc& a_bad_alloc) {
        exception = new ::ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what());
    } catch (const std::runtime_error& a_rte) {
        exception = new ::ev::Exception("C++ Runtime Error: %s\n", a_rte.what());
    } catch (const std::exception& a_std_exception) {
        exception = new ::ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what());
    } catch (...) {
        exception = new ::ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE());
    }
    
    looper_mutex_.lock();
    if ( nullptr != looper_ptr_ ) {
        delete looper_ptr_;
        looper_ptr_ = nullptr;
    }
    looper_mutex_.unlock();
    running_ = false;
    
    if ( nullptr != exception ) {
        OnFatalException(*exception);
        delete exception;
    }
    
    // TODO check if on fatal quit is already called
    bridge_->Quit();
}

