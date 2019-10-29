/**
* @file initializer.cc
*
* Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
*
* This file is part of casper-connectors..
*
* casper-connectors. is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* casper-connectors.  is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with casper-connectors..  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cc/global/initializer.h"

#ifndef CASPER_REQUIRE_GOOGLE_V8
    #include "cc/icu/initializer.h"
#else
    #include "cc/v8/singleton.h"
#endif

#include "cc/curl/initializer.h"

#include "cc/fs/dir.h"
#include "cc/fs/file.h"

#include "cc/types.h"
#include "cc/debug/types.h"

#include "sys/process.h"

#include "cc/exception.h"

#include "ev/signals.h"
#include "ev/logger.h"
#include "ev/logger_v2.h"

#include <locale.h>         // setlocale
#include "unicode/locid.h"  // locale

#include <event2/event.h>  // event_get_version
#include <event2/thread.h> // evthread_use_pthreads

#include <map>

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::global::OneShot::OneShot (cc::global::Initializer& a_instance)
    : ::cc::Initializer<::cc::global::Initializer>(a_instance)
{
    instance_.process_       = nullptr;
    instance_.directories_    = nullptr;
    instance_.loggable_data_ = nullptr;
    instance_.warmed_up_     = false;
    instance_.initialized_   = false;
}

/**
 * @brief Destructor.
 */
cc::global::OneShot::~OneShot ()
{
    if ( nullptr != instance_.loggable_data_ ) {
        delete instance_.loggable_data_;
    }
    if ( nullptr != instance_.directories_ ) {
        delete instance_.directories_;
    }
    if ( nullptr != instance_.process_ ) {
        delete instance_.process_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

#define CC_GLOBAL_INITIALIZER_KEY_FMT "%-18s:"

/**
 * @brief Warmup casper-connectors.
 *
 * @param a_process      [REQUIRED] Process information, see \link cc::global::Initializer::Process \link.
 * @param a_directories  [OPTIONAL] directories information, see \link cc::global::Initializer::Directories \link.
 * @param a_logs         [REQUIRED] Logs to be registered.
 * @param a_next_step    [REQUIRED] Function & arguments to call / pass before exiting this function call.
 * @param a_debug_tokens [OPTIONAL] Debug tokens to register.
 */
void cc::global::Initializer::WarmUp (const cc::global::Process& a_process,
                                      const cc::global::Directories* a_directories,
                                      const cc::global::Logs& a_logs,
                                      const cc::global::Initializer::V8& a_v8,
                                      const cc::global::Initializer::WarmUpNextStep& a_next_step,
                                      const std::set<std::string>* a_debug_tokens)
{
    // ... can't start if alread initialized ...
    if ( true == warmed_up_ ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__);
    }
    
    // ... mark as 'main' thread ...
    OSALITE_DEBUG_SET_MAIN_THREAD_ID();
    
    process_ = new cc::global::Process(
        {
            /* alt_name_  */ a_process.alt_name_,
            /* name_      */ a_process.name_,
            /* version_   */ a_process.version_,
            /* info_      */ a_process.info_,
            /* pid_       */ getpid(),
            /* is_master_ */ a_process.is_master_
        }
    );
        
    // ... create loggable data ...
    loggable_data_ = new ::ev::Loggable::Data(/* owner_ptr_ */ this,
                                              /* ip_addr_   */ "127.0.0.1",
                                              /* module_    */ process_->info_,
                                              /* tag_       */ ""
    );
    
    if ( nullptr != a_directories ) {
        directories_ = new Directories(*a_directories);
    } else {
        const std::string process_name = ( 0 != process_->alt_name_.length() ? process_->alt_name_ : process_->name_ );
        directories_ = new Directories({
            /* etc_   */ cc::fs::Dir::Normalize("/usr/local/etc/"      + process_name),
            /* log_   */ cc::fs::Dir::Normalize("/usr/local/var/log/"  + process_name),
            #ifdef __APPLE__
            /* share_ */ cc::fs::Dir::Normalize("/usr/local/share/"    + process_name),
            #else
                /* share_ */ cc::fs::Dir::Normalize("/usr/share/"      + process_name),
            #endif
            /* run_   */ cc::fs::Dir::Normalize("/usr/local/var/run/"  + process_name),
            /* lock_  */ cc::fs::Dir::Normalize("/usr/local/var/lock/" + process_name),
            /* tmp_   */ cc::fs::Dir::Normalize("/tmp/")
        });
    }
    
    // ... ensure essential directories exists ...
    for ( auto& dir : { &directories_->log_, &directories_->run_, &directories_->lock_ } ) {
        if ( false == cc::fs::Dir::Exists(*dir) ) {
            cc::fs::Dir::Make(*dir);
        }
    }
                
    try {
        
        //
        // ... initialize debug trace ...
        //
        osal::debug::Trace::GetInstance().Startup();

        // TODO 2.0

        #ifdef __APPLE__
                FILE* where = stdout;
        #else
                FILE* where = stderr;
        #endif

        if ( nullptr != a_debug_tokens ) {
            for ( auto token : *a_debug_tokens ) {
                OSALITE_REGISTER_DEBUG_TOKEN(token, where);
            }
        }
        
        // .. global status ...
        osal::debug::Trace::GetInstance().Register("status", where);
                  
        // ... starting up ...
        Log("status", "\n* %s - configuring %s process w/pid %u...\n",
             process_->info_.c_str(), ( true == process_->is_master_ ? "master" : "worker" ), process_->pid_
        );
                
        //
        // ... configuration ...
        //
        
        Log("status", "\n\t⌥ CONFIGURATION\n");
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "Target", CC_IF_DEBUG_ELSE("debug", "release"));
        
        //
        // ... directories ...
        //
        const std::map<std::string, const std::string&> directories = {
            { "etc"  , directories_->etc_     },
            { "log"  , directories_->log_     },
            { "share", directories_->share_   },
            { "run"  , directories_->run_     },
            { "lock" , directories_->lock_    },
            { "tmp"  , directories_->tmp_     }
        };
        
        Log("status", "\n\t⌥ DIRECTORIES\n");
        for ( auto it : directories ) {
            Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", it.first.c_str(), it.second.c_str());
        }

        //
        // ... logs ...
        //
        ::ev::Logger::GetInstance().Startup();
        ::ev::LoggerV2::GetInstance().Startup();
        
        EnableLogsIfRequired(a_logs);
        
        //
        // ... c funtions - system locale ...
        //

        // ... set the initial seed value for future calls to random(). ...
        struct timeval tv;
        if ( 0 != gettimeofday(&tv, NULL) ) {
            throw ::cc::Exception("Unable set the initial seed value for future calls to random()!");
        }
        srandom(((unsigned) process_->pid_ << 16) ^ (unsigned)tv.tv_sec ^ (unsigned)tv.tv_usec);
        
        // ... set locale must be called @ main(int argc, char** argv) ...
        const char* lc_all = setlocale (LC_ALL, NULL);
        
        setlocale (LC_NUMERIC, "C");
        
        const char* lc_numeric = setlocale (LC_NUMERIC, NULL);

        Log("status", "\n\t⌥ LOCALE\n");
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n"     , "LC_ALL"    , lc_all);
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s - " DOUBLE_FMT "\n", "LC_NUMERIC", lc_numeric, (double)123.456);

        CC_IF_DEBUG(
            //
            // ... *printf function ...
            //
            if ( true == process_->is_master_ ) {
                Log("status", "\n\t⌥ *printf(...)\n");
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "SIZET_FMT" , SIZET_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT8_FMT"  , INT8_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT8_FMT" , UINT8_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT16_FMT" , INT16_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT16_FMT", UINT16_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT32_FMT" , INT32_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT32_FMT", UINT32_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT64_FMT" , INT64_FMT);
                Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT64_FMT", UINT64_FMT);
            }
        );
        
        //
        // ... ICU / V8 ...
        //
        
        const std::string icu_dat_file_uri = ::cc::fs::Dir::Normalize(directories_->share_) + CC_IF_DEBUG_ELSE("v8/debug/icudtl.dat", "/v8/icudtl.dat");
        
        // ... LIBEVENT2 ...
        Log("status", "\n\t⌥ LIBEVENT2\n");
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", event_get_version());
        const int evthread_use_pthreads_rv = evthread_use_pthreads();
        if ( 0 == evthread_use_pthreads_rv ) {
            Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "PTHREADS", "OK");
        } else {
            throw ::cc::Exception("Unable to initialize libevent2, error code is %d", (int)evthread_use_pthreads_rv);
        }

    #ifndef CASPER_REQUIRE_GOOGLE_V8

        // ... ICU ...
        
        Log("status", "\n\t⌥ ICU\n");
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", U_ICU_VERSION);
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "DATA FILE", icu_dat_file_uri.c_str());
        const UErrorCode icu_error_code = ::cc::icu::Initializer::GetInstance().Load(icu_dat_file_uri);
        if ( UErrorCode::U_ZERO_ERROR == icu_error_code ) {
            Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " OK\n", "INIT");
        } else {
            throw ::cc::Exception("Unable to initialize ICU, error code is %d", (int)icu_error_code);
        }

        if ( true == a_v8.required_ ) {
            throw ::cc::Exception("V8 is required but initializer was not compiled with V8 support!");
        }
        
    #else

        // ... V8 ...
        if ( true == a_v8.required_ ) {
            Log("status", "\n\t⌥ V8\n");
            Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", ::v8::V8::GetVersion());
            cc::v8::Singleton::GetInstance().Startup(/* a_exec_uri     */ sys::Process::GetExecURI(process_->pid_).c_str(),
                                                     /* a_icu_data_uri */ icu_dat_file_uri.c_str()
            );
        }
        if ( true == a_v8.runs_on_main_thread_ ) {
            ::cc::v8::Singleton::GetInstance().Initialize();
        }
        
        // ... ICU ...
        Log("status", "\n\t⌥ ICU\n");
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", U_ICU_VERSION);
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "DATA FILE", icu_dat_file_uri.c_str());

    #endif
            
        // ... ensure required locale(s) is / are supported ...
        const U_ICU_NAMESPACE::Locale icu_default_locale = uloc_getDefault();
        
        UErrorCode icu_set_locale_error = U_ZERO_ERROR;
        
        // ... check if locale is supported
        for ( auto locale : { "pt_PT", "en_UK" } ) {
            U_ICU_NAMESPACE::Locale::setDefault(U_ICU_NAMESPACE::Locale::createFromName(locale), icu_set_locale_error);
            if ( U_FAILURE(icu_set_locale_error) ) {
                throw ::cc::Exception("Error while initializing ICU: %s locale is not supported!", locale);
            }
            const U_ICU_NAMESPACE::Locale test_locale = uloc_getDefault();
            if ( 0 != strcmp(locale, test_locale.getBaseName()) ) {
                throw ::cc::Exception("Error while initializing ICU: %s locale is not supported!", locale);
            }
        }
        // ... rollback to default locale ...
        U_ICU_NAMESPACE::Locale::setDefault(icu_default_locale, icu_set_locale_error);
        if ( U_FAILURE(icu_set_locale_error) ) {
            throw ::cc::Exception("Error while initializing ICU: unable to rollback to default locale!");
        }
            
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "LOCALE", icu_default_locale.getBaseName());
        
        //
        // .. cURL ...
        //
        
        Log("status", "\n\t⌥ cURL\n");
        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", LIBCURL_VERSION);
        const CURLcode curl_init_rv = cc::curl::Initializer::GetInstance().Start();
        if ( CURLE_OK == curl_init_rv ) {
            Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " OK\n", "INIT");
        } else {
            throw ::cc::Exception("Unable to initialize cURL, error code is %d", (int)curl_init_rv);
        }
                 
        //
        // ... process specific initialization ...
        //
        
        ::cc::global::Logs other_logs;

        a_next_step.function_(*process_, *directories_, a_next_step.args_, other_logs);
        
        EnableLogsIfRequired(other_logs);
        
        // ... mark as warmed up ...
        warmed_up_ = true;

        // ... done ..
        Log("status", "\n* %s - %s process w/pid %u configured...\n",
            process_->info_.c_str(), ( true == process_->is_master_ ? "master" : "worker" ), process_->pid_
        );

    } catch (const std::overflow_error& a_of_e) {
        Log("status", "\n* std::overflow_error: %s\n", a_of_e.what());
        exit(-1);
    } catch (const std::bad_alloc& a_bad_alloc) {
        Log("status", "\n* std::bad_alloc: %s\n", a_bad_alloc.what());
        exit(-1);
    } catch (const std::runtime_error& a_rt_e) {
        Log("status", "\n* std::runtime_error: %s\n", a_rt_e.what());
        exit(-1);
    } catch (const std::exception& a_std_exception) {
        Log("status", "\n* std::exception: %s\n", a_std_exception.what());
        exit(-1);
    } catch (...) {
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (::cc::Exception& a_cc_exception) {
            Log("status", "\n* %s\n", a_cc_exception.what());
        }
        exit(-1);
    }
}


/**
 * @brief Initialize casper-connectors.
 * *
 * @param a_signals      [REQUIRED] Signals to register and callback, see \link cc::global::Initializer::Signals \link.
 * @param a_callbacks    [REQUIRED] A set o function that can be called through this object life cycle.
 */
void cc::global::Initializer::Startup (const cc::global::Initializer::Signals& a_signals,
                                       const cc::global::Initializer::Callbacks& a_callbacks)
{
    
    Log("status", "* %s - %s process w/pid %d is starting up...\n",
        process_->info_.c_str(), ( true == process_->is_master_ ? "master" : "worker" ), (int)process_->pid_
    );

    // ... can't start if already initialized ...
    if ( true == initialized_ ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__);
    }

    // ... can't start if not warmed up ...
    if ( false == warmed_up_ ) {
        throw ::cc::Exception("Logic error - cc::global::Initializer::WarmUp not called yet!");
    }
        
    //
    // ... signal handing ...
    //
    ::ev::Signals::GetInstance().Startup(*loggable_data_, a_signals.register_,
            /* a_callbacks */
            {
                /* on_signal_           */ a_signals.unhandled_signals_callback_,
                /* on_fatal_exception_  */ a_callbacks.on_fatal_exception_,
                /* call_on_main_thread_ */ a_callbacks.call_on_main_thread_
            }
    );

    // ... mark as initialized ...
    initialized_ = true;

    Log("status", "* %s - %s process w/pid %d is started up...\n",
        process_->info_.c_str(), ( true == process_->is_master_ ? "master" : "worker" ), (int)process_->pid_
    );
}

/**
 * @brief Dealloc previously allocated memory ( if any ).
 *
 * @param a_for_cleanup_only True if it's for clean up.
 */
void cc::global::Initializer::Shutdown (bool a_for_cleanup_only)
{
    Log("status", "* %s with pid %d is %s...\n",
        process_->info_.c_str(), (int)process_->pid_, true == a_for_cleanup_only ? "cleaning" : "shutting down"
    );

    // ... release loggable data ...
    if ( nullptr != loggable_data_ ) {
        delete loggable_data_;
        loggable_data_ = nullptr;
    }
    
    const cc::global::Process process = *process_;
    // ... release process data ...
    if ( nullptr != process_ ) {
        delete process_;
        process_ = nullptr;
    }

    //
    // ... shutdown singletons?
    //
    
    // .... signals ...
    ::ev::Signals::GetInstance().Shutdown();
    
    // ... logger ...
    ::ev::Logger::GetInstance().Shutdown();
    ::ev::LoggerV2::GetInstance().Shutdown();

    // ... debug trace ...
    osal::debug::Trace::GetInstance().Shutdown();

    //
    // ... destroy singletons?
    //
    if ( false == a_for_cleanup_only ) {
        // ... cURL ...
        cc::curl::Initializer::Destroy();
        // ... ICU // V8 ...
        #ifndef CASPER_REQUIRE_GOOGLE_V8
            // ... ICU ...
            ::cc::icu::Initializer::Destroy();
        #else
            // ... ICU // V8 ...
            cc::v8::Singleton::Destroy();
        #endif
        // ... logger ...
        ::ev::Logger::Destroy();
        ::ev::LoggerV2::Destroy();
        
        // ... debug trace ...
        osal::debug::Trace::Destroy();

        // .... signals ...
        ::ev::Signals::Destroy();
    }
    
    // ... mark as NOT initialized ...
    initialized_ = false;
    
    Log("status", "* %s with pid %d is %s ...\n",
        process.info_.c_str(), (int)process.pid_, true == a_for_cleanup_only ? "cleaned up" : "down"
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Enable logs ( if required ).
 *
 * @param a_logs Logs settings, see \link cc::global::Logs \link.
 */
void cc::global::Initializer::EnableLogsIfRequired (const cc::global::Logs& a_logs)
{
    if ( 0 == a_logs.size() ) {
        return;
    }
    
    size_t enabled_count = 0;

    for ( auto& entry : a_logs ) {
        // ... if not enabled ...
        if ( false == entry.enabled_ ) {
            // ... next token ...
            continue;
        }
        // ... if it's a conditional enabler ...
        if ( true == entry.conditional_ ) {
            // ... .enabled file must exists ...
            const std::string flag = ( directories_->log_ + entry.token_ + ".enabled" );
            if ( false == ::cc::fs::File::Exists(flag) ) {
                // ... it does not, next token ...
                continue;
            }
        }
        
        enabled_count++;
        if ( 1 == enabled_count ) {
            Log("status", "\n\t⌥ LOGS\n");
        }
        
        // ... all conditions are met to register token ...
        const std::string uri = ( entry.uri_.length() > 0 ? entry.uri_ : ( directories_->log_ + entry.token_ + ".log" ) ) ;

        Log("status", "\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " [" UINT8_FMT "] %s\n", entry.token_.c_str(), entry.version_, uri.c_str());
        
        switch(entry.version_) {
            case 0:
                ::osal::debug::Trace::GetInstance().Register(entry.token_, stdout);
                break;
            case 1:
                ::ev::Logger::GetInstance().Register(entry.token_, uri);
                break;
            case 2:
                ::ev::LoggerV2::GetInstance().Register(entry.token_, uri);
                break;
            default:
                throw ::cc::Exception("Unsupported logger version %d", entry.version_);
        }
    }
}

/**
 * @brief Log a message.
 *
 * @param a_token  Previously registered token
 * @param a_format Message format.
 * @param ...
 */
void cc::global::Initializer::Log (const char* a_token, const char* a_format, ...)
{
    // TODO 2.0 - + replace osal::debug::trace
#if 1
    va_list args;
    va_start(args, a_format);
#ifdef __APPLE__
    vfprintf(stdout, a_format, args);
    fflush(stdout);
#else
    vfprintf(stderr, a_format, args);
    fflush(stderr);
#endif
    va_end(args);
#endif
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @returns True if V8 is supported, false otherwise.
 */
bool cc::global::Initializer::SupportsV8 ()
{
#ifdef CASPER_REQUIRE_GOOGLE_V8
    return true;
#else
    return false;
#endif
}
