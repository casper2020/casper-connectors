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

#include "ev/signals.h"

#include "ev/logger.h"

#include "ev/redis/device.h"
#include "ev/redis/subscriptions/manager.h"

#include "ev/postgresql/device.h"
#include "ev/curl/device.h"

#include "osal/osalite.h"

#include "osal/debug/trace.h"

#include "cc/utc_time.h"

#include "unicode/locid.h"  // locale
#include "unicode/utypes.h" // u_init
#include "unicode/uclean.h" // u_cleanup

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
            /* host_          */ "127.0.0.1",
            /* port_          */  11300,
            /* timeout_       */  0.0,
            /* tubes_         */ {},
            /* abort_polling_ */ 3.0
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
    // ... set locale must be called @ main(int argc, char** argv) ...
    const char* lc_all = setlocale (LC_ALL, NULL);
    
    setlocale (LC_NUMERIC, "C");
    
    const char* lc_numeric = setlocale (LC_NUMERIC, NULL);
    
    const pid_t process_pid = getpid();
    
    OSALITE_DEBUG_TRACE("startup",
                        "[%s] - pid " UINT64_FMT " - is starting up...",
                        __PRETTY_FUNCTION__, process_pid
    );
    
    // ... set the initial seed value for future calls to random(). ...
    struct timeval tv;
    if ( 0 != gettimeofday(&tv, NULL) ) {
        throw ev::Exception("Unable set the initial seed value for future calls to random()!");
    }
    srandom(((unsigned) process_pid << 16) ^ (unsigned)tv.tv_sec ^ (unsigned)tv.tv_usec);
    
    // ... mark as 'main' thread ...
    OSALITE_DEBUG_SET_MAIN_THREAD_ID();
   
    //
    // Copy Startup Config
    //
    startup_config_ = new ev::loop::beanstalkd::Runner::StartupConfig({
        /* name_           */ a_config.name_,
        /* versioned_name_ */ a_config.versioned_name_,
        /* instance_       */ a_config.instance_,
        /* exec_path_      */ a_config.exec_path_,
        /* conf_file_uri_  */ a_config.conf_file_uri_
    });
    
    //
    // Work directories:
    //
    shared_config_->directories_ = {
#ifdef __APPLE__
        /* log_    */ "/usr/local/var/log/"  + a_config.name_ + "/",
        /* run_    */ "/usr/local/var/run/"  + a_config.name_ + "/",
        /* lock_   */ "/usr/local/var/lock/" + a_config.name_ + "/",
        /* shared_ */ "/usr/local/share/"    + a_config.name_ + "/",
#else
        /* log_    */ "/var/log/"            + a_config.name_ + "/",
        /* run_    */ "/var/run/"            + a_config.name_ + "/",
        /* lock_   */ "/var/lock/"           + a_config.name_ + "/",
        /* shared_ */ "/usr/share/"          + a_config.name_ + "/",
#endif
        /* output_ */ "/tmp/"
    };

    //
    // Load config:
    //

    std::ifstream f_stream(a_config.conf_file_uri_);
    if ( false == f_stream.is_open() ) {
        throw ev::Exception("Unable to open configuration file '%s'!", a_config.conf_file_uri_.c_str());
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
            const Json::Value& tubes = beanstalkd["tubes"];
            if ( false == tubes.isArray() ) {
                throw ev::Exception("An error occurred while loading configuration - invalid tubes type!");
            }
            if ( tubes.size() > 0 ) {
                shared_config_->beanstalk_.tubes_.clear();
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
    
    for ( auto dir : { &shared_config_->directories_.log_, &shared_config_->directories_.run_, &shared_config_->directories_.lock_ } ) {
        if ( osal::Dir::Status::EStatusOk != osal::Dir::CreateDir(dir->c_str()) ) {
            throw ev::Exception("Unable to create directory '%s'!", dir->c_str());
        }
    }
    
    //
    // Write pid file for SYSTEMD:
    //
    const std::string pid_file = shared_config_->directories_.run_ + std::to_string(a_config.instance_) + ".pid";
    FILE* pid_fd = fopen(pid_file.c_str(), "w");
    if ( nullptr == pid_fd ) {
        throw ev::Exception("Unable to open file '%s' to write pid: nullptr", pid_file.c_str());
    } else {
        const int bytes_written = fprintf(pid_fd, "%d", process_pid);
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
                                              /* module_    */ a_config.versioned_name_,
                                              /* tag_       */ "instance-" + std::to_string(a_config.instance_)
    );
    // ... set a http client ...
    http_          = new ev::curl::HTTP(*loggable_data_);
    
    //                                      //
    // ... initialize shared singletons ... //
    //                                      //
    // ... Debug Logger(s) ...
    osal::debug::Trace::GetInstance().Startup();
    
    // ... Permanent Logger(s) ...
    ::ev::Logger::GetInstance().Startup();
    for ( auto it : shared_config_->log_tokens_ ) {
        ::ev::Logger::GetInstance().Register(it.first, it.second);
    }

    // ... V8 ...
#if defined(__APPLE__) && ( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
    const std::string icu_data_uri = shared_config_->directories_.shared_ + "/v8/debug/icudtl.dat";
#else
    const std::string icu_data_uri = shared_config_->directories_.shared_ + "/v8/icudtl.dat";
#endif
    cc::v8::Singleton::GetInstance().Startup(/* a_exec_uri     */  a_config.exec_path_.c_str(),
                                             /* a_icu_data_uri */ icu_data_uri.c_str()
    );
    
    //
    // ICU
    //
#ifdef USE_SYSTEM_ICU
    UErrorCode icu_error_code = UErrorCode::U_ZERO_ERROR;
    u_init(&icu_error_code);
    if ( UErrorCode::U_ZERO_ERROR != icu_error_code ) {
        throw ev::Exception("Error while initializing ICU: error code %d", (int)icu_error_code);
    }
#endif // if not, should be already initialized by v8
    
    // ... ensure required locale(s) is / are supported ...
    const U_ICU_NAMESPACE::Locale icu_default_locale = uloc_getDefault();
    
    UErrorCode icu_set_locale_error = U_ZERO_ERROR;
    
    // ... check if locale is supported
    for ( auto locale : { "pt_PT", "en_UK" } ) {
        U_ICU_NAMESPACE::Locale::setDefault(U_ICU_NAMESPACE::Locale::createFromName(locale), icu_set_locale_error);
        if ( U_FAILURE(icu_set_locale_error) ) {
            throw ev::Exception("Error while initializing ICU: %s locale is not supported!", locale);
        }
        const U_ICU_NAMESPACE::Locale test_locale = uloc_getDefault();
        if ( 0 != strcmp(locale, test_locale.getBaseName()) ) {
            throw ev::Exception("Error while initializing ICU: %s locale is not supported!", locale);
        }
    }
    // ... rollback to default locale ...
    U_ICU_NAMESPACE::Locale::setDefault(icu_default_locale, icu_set_locale_error);
    if ( U_FAILURE(icu_set_locale_error) ) {
        throw ev::Exception("Error while setting ICU: unable to rollback to default locale!");
    }
    
    //                                               //
    // ... LOG IMPORTANT INITIALIZATION SETTINGS ... //
    //                                               //
    osal::debug::Trace::GetInstance().Log("status", "\n* %s - starting up process w/pid %u...\n",
                                          a_config.name_.c_str(), process_pid
    );

#if defined(NDEBUG) && !( defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG) )
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "Target", "release");
#else
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "Target", "debug");
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "SIZET_FMT" , SIZET_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "INT8_FMT"  , INT8_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "UINT8_FMT" , UINT8_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "INT16_FMT" , INT16_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "UINT16_FMT", UINT16_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "INT32_FMT" , INT32_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "UINT32_FMT", UINT32_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "INT64_FMT" , INT64_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "UINT64_FMT", UINT64_FMT);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "DOUBLE_FMT", DOUBLE_FMT);
#endif

    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "IUC D LOC" , icu_default_locale.getBaseName());

    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n"     , "LC_ALL"    , lc_all);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s - " DOUBLE_FMT "\n", "LC_NUMERIC", lc_numeric, (double)123.456);

    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: " UINT64_FMT "\n", "MTID", osal::ThreadHelper::GetInstance().CurrentThreadID());

    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n", "UTC TIME", ::cc::UTCTime::NowISO8601DateTime().c_str());

    osal::debug::Trace::GetInstance().Log("status", "* %s - process w/pid %u configured...\n",
                                          a_config.name_.c_str(), process_pid
    );
    
    //
    // SOCKETS:
    //
    std::stringstream ss;
    
    // ... prepare socket file names ...
    ss.str("");
    ss << shared_config_->directories_.run_ << "ev-scheduler." << process_pid << ".socket";
    const std::string scheduler_socket_fn = ss.str();
    
    ss.str("");
    ss << shared_config_->directories_.run_ << "ev-shared-handler." << process_pid << ".socket";
    const std::string  shared_handler_socket_fn = ss.str();
    
    //
    // BRIDGE
    //
    bridge_ = new ev::loop::Bridge();
    (void)bridge_->Start(shared_handler_socket_fn, a_fatal_exception_callback);
    
    //
    // SCHEDULER
    //
    osal::ConditionVariable scheduler_cv;
    // ... then initialize scheduler ...
    ::ev::scheduler::Scheduler::GetInstance().Start(scheduler_socket_fn,
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
    
    // ... register to handle signal tasks ...
    ::ev::Signals::GetInstance().Register(bridge_, std::bind(&ev::loop::beanstalkd::Runner::OnFatalException, this, std::placeholders::_1));
    
    
    //
    // Install Signal Handler
    //
    ::ev::Signals::GetInstance().Startup(*loggable_data_);
    ::ev::Signals::GetInstance().Register(
                                          /* a_signals */
                                          {SIGUSR1, SIGTERM, SIGQUIT, SIGTTIN},
                                          /* a_callback */
                                          [this](const int a_sig_no) {
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
    );
    
    // ... mark as initialized ...
    initialized_ = true;
    
    OSALITE_DEBUG_TRACE("startup",
                        "[%s] - pid " UINT64_FMT " - started up",
                        __PRETTY_FUNCTION__, process_pid
    );
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
    
    const pid_t process_pid = getpid();

    OSALITE_DEBUG_TRACE("startup",
                        "[%s] - pid " UINT64_FMT " - is shutting down...",
                        __PRETTY_FUNCTION__, process_pid
    );
    
    // ... first unregister from scheduler ...
    ::ev::Signals::GetInstance().Unregister();
    
    // ... clean up
    ::osal::ConditionVariable cleanup_cv;
    const auto cleanup = ([this, process_pid, a_sig_no, &cleanup_cv](){
        
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

        OSALITE_DEBUG_TRACE("startup",
                            "[%s] - pid " UINT64_FMT " - is down...",
                            __PRETTY_FUNCTION__, process_pid
        );        
        
        //
        // ... singletons shutdown ...
        //

        // ... debug trace cache ...
        osal::debug::Trace::GetInstance().Shutdown();
        // ... logger ...
        ev::Logger::GetInstance().Shutdown();
        // ... v8 ...
        ::cc::v8::Singleton::GetInstance().Shutdown();
        // ... signal handler ...
        // DOES NOT EXIST: ::ev::Signals::GetInstance().Shutdown();
        
        //
        // ... singletons destruction ...
        //

        // ... logger ...
        ev::Logger::Destroy();
        // ... debug trace ...
        osal::debug::Trace::Destroy();
        // ... v8 ...
        // DOES NOT EXIST: ::cc::v8::Singleton::Destroy();
        // ... signal handler ...
        ::ev::Signals::Destroy();

        // ... v8 ...
        // TODO ::cc::v8::Singleton::Destroy();
        
        //
        // third party libraries cleanup
        //
        // ... ICU ...
#ifdef USE_SYSTEM_ICU
        u_cleanup();
#endif
        
        // ...
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
    
    (void)process_pid;
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
        
        // ... initialize v8 ...
        ::cc::v8::Singleton::GetInstance().Initialize();
        
        consumer_cv_->Wake();
        
        looper = new ev::loop::beanstalkd::Looper(shared_config_->factory_,
                                                  /* a_callbacks */
                                                  {
                                                      /* on_fatal_exception_ */ std::bind(&ev::loop::beanstalkd::Runner::OnFatalException   , this, std::placeholders::_1),
                                                      /* on_main_thread_     */ std::bind(&ev::loop::beanstalkd::Runner::ExecuteOnMainThread, this, std::placeholders::_1, std::placeholders::_2),
                                                      /* on_submit_job_      */ std::bind(&ev::loop::beanstalkd::Runner::SubmitJob          , this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
                                                  },
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
    
    OnFatalException(*exception);
    delete exception;
    
    // TODO check if on fatal quit is already called
    bridge_->Quit();
}

