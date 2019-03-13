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

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 */
ev::loop::beanstalkd::Runner::Runner ()
{
    s_initialized_              = false;
    s_shutting_down_            = false;
    s_quit_                     = false;
    s_bridge_                   = nullptr;
    s_consumer_thread_          = nullptr;
    s_consumer_cv_              = nullptr;
    loggable_data_              = nullptr;
    s_fatal_exception_callback_ = nullptr;
    shared_config_              = new ev::loop::beanstalkd::Runner::SharedConfig({
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
            /* abort_polling_ */ 3
        },
        /* device_limits_ */ {},
        /* factories_ */ {}
      }
    );
    http_ = new ::ev::curl::HTTP();
}

/**
 * @brief Destructor.
 */
ev::loop::beanstalkd::Runner::~Runner ()
{
    if ( nullptr != looper_ ) {
        delete looper_;
    }
    s_fatal_exception_callback_ = nullptr;
    if ( nullptr != http_) {
        delete http_;
    }
    if ( nullptr != loggable_data_ ) {
        delete loggable_data_;
        loggable_data_ = nullptr;
    }
    // TODO
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
        /* name_          */ a_config.name_,
        /* instance_      */ a_config.instance_,
        /* exec_path_     */ a_config.exec_path_,
        /* conf_file_uri_ */ a_config.conf_file_uri_
    });
    
    //
    // Work directories:
    //
    shared_config_->directories_ = {
#ifdef __APPLE__
        /* log_  */ "/usr/local/var/log/"  + a_config.name_ + "/",
        /* run_  */ "/usr/local/var/run/"  + a_config.name_ + "/",
        /* lock_ */ "/usr/local/var/lock/" + a_config.name_ + "/"
#else
        /* log_  */ "/var/log/"  + a_config.name_ + "/",
        /* run_  */ "/var/run/"  + a_config.name_ + "/",
        /* lock_ */ "/var/lock/" + a_config.name_ + "/"
#endif
    };
    
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
    
    //
    // Load config:
    //
    // ... load data ...
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
    
    InnerStartup(*startup_config_, read_config, *shared_config_);
    
    // .. set loggable data ...
    loggable_data_ = new ::ev::Loggable::Data(/* owner_ptr_ */ this,
                                              /* ip_addr_   */ shared_config_->ip_addr_,
                                              /* module_    */ a_config.name_,
                                              /* tag_       */ "instance-" + std::to_string(a_config.instance_)
    );
    
    //
    // ... initialize shared singletons ...
    //
#ifdef __APPLE__
    #if defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG)
        const std::string share_path = "/usr/local/share/" + a_config.name_ + "/v8/debug";
    #else
        const std::string share_path = "/usr/local/share/" + a_config.name_ + "/v8/";
    #endif
#else // assuming linux
    const std::string share_path = "/usr/share/" + a_config.name_ + "/v8/";
#endif
    
    cc::v8::Singleton::GetInstance().Startup(
                                             /* a_exec_uri          */  a_config.exec_path_.c_str(),
                                             /* a_icu_data_uri      */ ( share_path + "/icudtl.dat" ).c_str()
    );

    
    ::ev::Logger::GetInstance().Startup();
    for ( auto it : shared_config_->log_tokens_ ) {
        ::ev::Logger::GetInstance().Register(it.first, it.second);
    }
    
    osal::debug::Trace::GetInstance().Startup();
    
    // .. global status ...
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
#endif
    
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s\n"     , "LC_ALL"    , lc_all);
    osal::debug::Trace::GetInstance().Log("status", "\t- %-12s: %s - " DOUBLE_FMT "\n", "LC_NUMERIC", lc_numeric, (double)123.456);
    

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
    // Track callbacks
    //
    s_fatal_exception_callback_ = a_fatal_exception_callback;
    
    //
    // BRIDGE
    //
    s_bridge_ = new ev::loop::Bridge();
    call_on_main_thread_ = s_bridge_->Start(shared_handler_socket_fn, s_fatal_exception_callback_);
    
    //
    // SCHEDULER
    //
    
    osal::ConditionVariable scheduler_cv;
    // ... then initialize scheduler ...
    ::ev::scheduler::Scheduler::GetInstance().Start(scheduler_socket_fn,
                                                    *s_bridge_,
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
                                                               s_bridge_,
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
    ::ev::Signals::GetInstance().Register(s_bridge_, s_fatal_exception_callback_);
    
    // ... mark as initialized ...
    s_initialized_ = true;
    
    OSALITE_DEBUG_TRACE("startup",
                        "[%s] - pid " UINT64_FMT " - started up",
                        __PRETTY_FUNCTION__, process_pid
    );
}

/**
 * @brief Run two loops: a consumer loop and an event loop ( 'main' ).
 *
 * @param a_factories
 * @param a_beanstalk_config
 */
void ev::loop::beanstalkd::Runner::Run ()
{
    s_consumer_cv_     = new osal::ConditionVariable();
    s_consumer_thread_ = new std::thread(&ev::loop::beanstalkd::Runner::ConsumerLoop, this);
    s_consumer_thread_->detach();
    
    s_consumer_cv_->Wait();
    
    s_bridge_->Loop();
    
    if ( nullptr != s_consumer_cv_ ) {
        delete s_consumer_cv_;
        s_consumer_cv_ = nullptr;
    }
    
    if ( nullptr != s_consumer_thread_ ) {
        delete s_consumer_thread_;
        s_consumer_thread_ = nullptr;
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
    if ( true == s_shutting_down_ ) {
        // ... yes ...
        return;
    }
    
    // ... no, but will be now ...
    s_shutting_down_ = true;
    
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
        if ( nullptr != s_bridge_ ) {
            s_bridge_->Stop(a_sig_no);
            delete s_bridge_;
            s_bridge_ = nullptr;
        }
        
        // ... consumer thread can now be release ...
        if ( nullptr != s_consumer_thread_ ) {
            delete s_consumer_thread_;
            s_consumer_thread_ = nullptr;
        }
        
        if ( nullptr != s_consumer_cv_ ) {
            delete s_consumer_cv_;
            s_consumer_cv_ = nullptr;
        }

        InnerShutdown();

        if ( nullptr != loggable_data_ ) {
            delete loggable_data_;
            loggable_data_ = nullptr;
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

        //
        // ... singletons destruction ...
        //

        // ... logger ...
        ev::Logger::Destroy();
        // ... debug trace ...
        osal::debug::Trace::Destroy();
        
        // ... v8 ...
        // TODO ::cc::v8::Singleton::Destroy();
        
        // ...
        // ... reset initialized flag ...
        s_initialized_ = false;
        s_quit_        = false;
        
        // ... done ...
        cleanup_cv.Wake();
        
    });
    
    if ( true == s_bridge_->IsRunning() ) {
        call_on_main_thread_(cleanup);
    } else {
        cleanup();
    }
    cleanup_cv.Wait();
    
    (void)process_pid;
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
        
        s_consumer_cv_->Wake();
        
        looper = new ev::loop::beanstalkd::Looper(shared_config_->factories_, shared_config_->default_tube_);
        looper->Run(*loggable_data_, shared_config_->beanstalk_, s_quit_);
        
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
    
    call_on_main_thread_([this, exception]() {
        s_fatal_exception_callback_(*exception);
        delete exception;
    });
    
    s_bridge_->Quit();
}

