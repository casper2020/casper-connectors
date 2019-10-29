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

#include "cc/v8/singleton.h"

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
    loggable_data_            = nullptr;
    shared_config_            = new ev::loop::beanstalkd::Runner::SharedConfig({
        /* default_tube_ */ "",
        /* ip_addr_ */ "",
        /* directories_ */ {
            /* log_  */ "",
            /* run_  */ "",
            /* lock_ */ "",
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
            /* abort_polling_     */ 3.0,
            /* tubes_             */ {} ,
            /* sessionless_tubes_ */ {},
            /* action_tubes_      */ {}
        },
        /* device_limits_ */ {},
        /* factory */ nullptr
    });
    http_ = nullptr;
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
        delete shared_config_;
    }
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
void ev::loop::beanstalkd::Runner::Startup (const ev::loop::beanstalkd::Runner::StartupConfig& a_config,
                                            ev::loop::beanstalkd::Runner::FatalExceptionCallback a_fatal_exception_callback)
{
    const pid_t process_pid = getpid();

    CC_IF_DEBUG_DECLARE_VAR(std::set<std::string>, debug_tokens);
    CC_IF_DEBUG(
        debug_tokens.insert("exceptions");
    );

    //
    // ... initialize casper-connectors // third party libraries ...
    //
    ::cc::global::Initializer::GetInstance().WarmUp(
        /* a_process */
        {
            /* alt_name_  */ "",
            /* name_      */ a_config.name_,
            /* version_   */ a_config.version_,
            /* info_      */ a_config.info_,
            /* pid_       */ process_pid,
            /* is_master_ */ true
        },
        /* a_directories */
        nullptr, /* using defaults */
        /* a_logs */
        {},
        /* a_v8 */
        {
            /* required_            */ true,
            /* runs_on_main_thread_ */ false
        },       
        /* a_next_step */
        {
            /* function_ */ std::bind(&ev::loop::beanstalkd::Runner::OnGlobalInitializationCompleted, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
            /* args_     */ (void*)(&a_config)
        },
        /* a_debug_tokens */
        CC_IF_DEBUG_ELSE(&debug_tokens, nullptr)
    );
    
    ::cc::global::Initializer::GetInstance().Startup(
        /* a_signals */
        {
            /* register_ */ { SIGQUIT, SIGTERM, SIGTTIN },
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
            },
            /* on_fatal_exception_     */ std::bind(&ev::loop::beanstalkd::Runner::OnFatalException, this, std::placeholders::_1)
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
    startup_config_ = new ev::loop::beanstalkd::Runner::StartupConfig(*(const ev::loop::beanstalkd::Runner::StartupConfig*)a_args);
    
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
            shared_config_->beanstalk_.host_    = beanstalkd.get("host"   , shared_config_->beanstalk_.host_   ).asString();
            shared_config_->beanstalk_.port_    = beanstalkd.get("port"   , shared_config_->beanstalk_.port_   ).asInt();
            shared_config_->beanstalk_.timeout_ = beanstalkd.get("timeout", shared_config_->beanstalk_.timeout_).asFloat();
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
        
        /* logs */
        const Json::Value logs = read_config.get("logs", Json::Value::null);
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
                // ... at macOS and if debug mode ...
#if defined(__APPLE__) && !defined(NDEBUG) && ( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
                // ... delete all log files ...
                osal::File::Delete(shared_config_->directories_.log_.c_str(), "*.log", nullptr);
#endif
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
                        shared_config_->log_tokens_[token] = shared_config_->directories_.log_ + token + "." + std::to_string(startup_config_->instance_) + ".log";
                    }
                }
            }
        }
        
        InnerStartup(*startup_config_, read_config, *shared_config_);

    } catch (const Json::Exception& a_json_exception) {
        throw ev::Exception("An error occurred while loading configuration: JSON exception %s!", a_json_exception.what());
    }
        
    // ... at macOS and if debug mode ...
#if defined(__APPLE__) && !defined(NDEBUG) && ( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
    // ... delete all log files ...
    osal::File::Delete(shared_config_->directories_.log_.c_str(), "*.log", nullptr);
    osal::File::Delete(shared_config_->directories_.run_.c_str(), "*.pid", nullptr);
    osal::File::Delete(shared_config_->directories_.run_.c_str(), "ev-*.socket", nullptr);
#else // linux
    // ... log or release rotate should take care of this ...
#endif
        
    //
    // Write pid file for SYSTEMD:
    //
    const std::string pid_file = shared_config_->directories_.run_ + std::to_string(startup_config_->instance_) + ".pid";
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
                                              /* tag_       */ "instance-" + std::to_string(startup_config_->instance_)
    );
    
    // ... set a http client ...
    http_ = new ev::curl::HTTP();
    
    //
    // ... set PERMANENT DEBUG log tokens ...
    //
#if 0
    CC_IF_DEBUG(
      // ... v0 ...
      for ( auto token : { "ev_bridge" } ) {
          o_logs.push_back({ /* token_ */ token ,/* uri_ */ "", /* conditional_ */ false, /* enabled_ */ true, /* version_ */ 0 });
      }
    );
#endif

    //
    // ... set PERMANENT log tokens  ...
    //
    
    // ... v1 ...
    for ( auto token : { "libpq-connections" } ) {
        if ( shared_config_->log_tokens_.end() != shared_config_->log_tokens_.find(token) ) {
            continue;
        }
        o_logs.push_back({ /* token_ */ token ,/* uri_ */ shared_config_->directories_.log_ + token + "." + std::to_string(startup_config_->instance_) + ".log", /* conditional_ */ false, /* enabled_ */ true, /* version_ */ 1 });
    }
    // ... v2 ...
    for ( auto token : { "signals" } ) {
        if ( shared_config_->log_tokens_.end() != shared_config_->log_tokens_.find(token) ) {
            continue;
        }
        o_logs.push_back({ /* token_ */ token ,/* uri_ */ shared_config_->directories_.log_ + token + "." + std::to_string(startup_config_->instance_) + ".log", /* conditional_ */ false, /* enabled_ */ true, /* version_ */ 2 });
    }

    //
    // ... set OPTIONAL log tokens  ...
    //
    for ( auto it : shared_config_->log_tokens_ ) {
        o_logs.push_back({ /* token_ */ it.first, /* uri_ */ it.second, /* conditional_ */ false,  /* enabled_ */ true, /* version_ */ 2 });
    }
    
    //
    // SOCKETS:
    //
    std::stringstream ss;
    
    // ... prepare socket file names ...
    ss.str("");
    ss << shared_config_->directories_.run_ << "ev-scheduler." << a_process.pid_ << ".socket";
    const std::string scheduler_socket_fn = ss.str();
    
    ss.str("");
    ss << shared_config_->directories_.run_ << "ev-shared-handler." << a_process.pid_ << ".socket";
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
                                                    [this, &scheduler_cv]() {
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
 */
void ev::loop::beanstalkd::Runner::Run ()
{
    consumer_cv_     = new osal::ConditionVariable();
    consumer_thread_ = new std::thread(&ev::loop::beanstalkd::Runner::ConsumerLoop, this);
    consumer_thread_->detach();
    
    consumer_cv_->Wait();
    
    bridge_->Loop();
    
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

        InnerShutdown();

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
 * @brief Submit a 'beanstalkd' job.
 *
 * @param a_tube
 * @param a_payload
 * @param a_ttr
 */
void ev::loop::beanstalkd::Runner::SubmitJob (const std::string& a_tube, const std::string& a_payload, const uint32_t& a_ttr)
{
    
    ::ev::beanstalk::Producer producer(shared_config_->beanstalk_, a_tube.c_str());
    
    const int64_t status = producer.Put(a_payload, /* a_priority = 0 */ 0, /* a_delay = 0 */ 0, a_ttr);
    if ( status < 0 ) {
        throw ::ev::Exception("Beanstalk producer returned with error code " INT64_FMT "!",
                              status
        );
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
 * @brief Report a faltal exception.
 *
 * @param a_exception
 */
void ev::loop::beanstalkd::Runner::OnFatalException (const ev::Exception& a_exception)
{
    ExecuteOnMainThread([this, a_exception]{
        bridge_->ThrowFatalException(a_exception);
    }, /* a_blocking */ false);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Print queue consumer loop.
 */
void ev::loop::beanstalkd::Runner::ConsumerLoop ()
{
    ev::Exception*                  exception = nullptr;
    ev::loop::beanstalkd::Looper*   looper    = nullptr;
        
    try {
        
        ::cc::threading::Worker::SetName(startup_config_->abbr_ + "::Runner");
        ::cc::threading::Worker::BlockSignals({SIGTTIN, SIGTERM, SIGQUIT});
        
        const ev::loop::beanstalkd::Job::MessagePumpCallbacks callbacks = {
            /* on_fatal_exception_ */ std::bind(&ev::loop::beanstalkd::Runner::OnFatalException   , this, std::placeholders::_1),
            /* on_main_thread_     */ std::bind(&ev::loop::beanstalkd::Runner::ExecuteOnMainThread, this, std::placeholders::_1, std::placeholders::_2),
            /* on_submit_job_      */ std::bind(&ev::loop::beanstalkd::Runner::SubmitJob          , this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        };
        
        // ... initialize v8 ...
        ::cc::v8::Singleton::GetInstance().Initialize();
        
        consumer_cv_->Wake();
        
        looper = new ev::loop::beanstalkd::Looper(shared_config_->factory_,
                                                  callbacks,
                                                  shared_config_->default_tube_
        );
        looper->Run(*loggable_data_, shared_config_->beanstalk_, shared_config_->directories_.output_, quit_);
        
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
    
    if ( nullptr != looper ) {
        delete looper;
        looper = nullptr;
    }
    
    if ( nullptr != exception ) {
        OnFatalException(*exception);
        delete exception;
    }
    
    // TODO check if on fatal quit is already called
    bridge_->Quit();
}

