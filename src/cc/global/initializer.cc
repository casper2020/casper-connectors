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

#include "cc/icu/includes.h"

#include <event2/event.h>  // event_get_version
#include <event2/thread.h> // evthread_use_pthreads

#include "cc/logs/basic.h"

#include "osal/debug/trace.h"

#include <map>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h> // SSLeay_version // SSLEAY_VERSION

#ifdef __APPLE__
    #include "sys/bsd/process.h" // IsProcessBeingDebugged
#endif

#ifdef CC_GLOBAL_INITIALIZER_LOGGER_REGISTER
    #undef CC_GLOBAL_INITIALIZER_LOGGER_REGISTER
#endif
#define CC_GLOBAL_INITIALIZER_LOGGER_REGISTER(a_token, a_where) \
    if ( false == being_debugged_ ) { \
        cc::logs::Basic::GetInstance().Register(a_token, a_where); \
    }

#ifdef CC_GLOBAL_INITIALIZER_LOG
    #undef CC_GLOBAL_INITIALIZER_LOG
#endif
#define CC_GLOBAL_INITIALIZER_LOG(a_token, a_format, ...) \
    if ( false == being_debugged_ ) { \
        cc::logs::Basic::GetInstance().Log(a_token, a_format, __VA_ARGS__); \
    } else { \
        fprintf(stdout, a_format, __VA_ARGS__); \
        fflush(stdout); \
    }
    
/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
 */
cc::global::OneShot::OneShot (cc::global::Initializer& a_instance)
    : ::cc::Initializer<::cc::global::Initializer>(a_instance)
{
    instance_.process_        = nullptr;
    instance_.directories_    = nullptr;
    instance_.loggable_data_  = nullptr;
    instance_.warmed_up_      = false;
    instance_.initialized_    = false;
#ifdef __APPLE__
    instance_.being_debugged_ = sys::bsd::Process::IsProcessBeingDebugged(getpid());
#else
    instance_.being_debugged_ = false;
#endif
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
 * @param a_present      [REQUIRED]
 * @param a_debug_tokens [OPTIONAL] Debug tokens to register.
 */
void cc::global::Initializer::WarmUp (const cc::global::Process& a_process,
                                      const cc::global::Directories* a_directories,
                                      const cc::global::Logs& a_logs,
                                      const cc::global::Initializer::V8& a_v8,
                                      const cc::global::Initializer::WarmUpNextStep& a_next_step,
                                      const std::function<void(std::string&, std::map<std::string, std::string>&)> a_present,
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
            /* rel_date_  */ a_process.rel_date_,
            /* info_      */ a_process.info_,
            /* banner_    */ a_process.banner_,
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
        #ifdef __APPLE__
            const std::string prefix = "/usr/local";
        #else
            const std::string prefix = "";
        #endif
        directories_ = new Directories({
            /* etc_   */ cc::fs::Dir::Normalize(prefix + "/etc/"      + process_name),
            /* log_   */ cc::fs::Dir::Normalize(prefix + "/var/log/"  + process_name),
            #ifdef __APPLE__
            /* share_ */ cc::fs::Dir::Normalize(prefix + "/share/"    + process_name),
            #else
                /* share_ */ cc::fs::Dir::Normalize("/usr/share/"      + process_name),
            #endif
            /* run_   */ cc::fs::Dir::Normalize(prefix + "/var/run/"  + process_name),
            /* lock_  */ cc::fs::Dir::Normalize(prefix + "/var/lock/" + process_name),
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
        
        cc::logs::Basic::GetInstance().Startup();
        
        //
        // ... initialize debug trace ...
        //
        // TODO 2.0
        osal::debug::Trace::GetInstance().Startup();

        #ifdef __APPLE__
            FILE* where = stdout;
        #else // assuming linux
            FILE* where = stderr;
        #endif
                
        if ( nullptr != a_debug_tokens ) {
            for ( auto token : *a_debug_tokens ) {
                OSALITE_REGISTER_DEBUG_TOKEN(token, where);
            }
        }
        
        // .. global status ...
        CC_GLOBAL_INITIALIZER_LOGGER_REGISTER("cc-status", directories_->log_ + "cc-status.log");
        if ( true == process_->is_master_ && process_->banner_.length() > 0 ) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status", "\n%s\n", process_->banner_.c_str());
        }
        
        // ... starting up ...
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* %s - configuring %s process w/pid %u...\n",
             process_->info_.c_str(), ( true == process_->is_master_ ? "master" : "worker" ), process_->pid_
        );
                
        //
        // ... configuration ...
        //
        std::string process_name_uc = process_->name_;
        std::transform(process_name_uc.begin(), process_name_uc.end(), process_name_uc.begin(), ::toupper);
        
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", process_name_uc.c_str());
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION"      , process_->version_.c_str());
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "RELEASE DATE" , process_->rel_date_.c_str());
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INFO"         , process_->info_.c_str());
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "TARGET"       , CC_IF_DEBUG_ELSE("debug", "release"));
        
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
        
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "DIRECTORIES");
        for ( auto it : directories ) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", it.first.c_str(), it.second.c_str());
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
        
        const char* c_all_tmp = setlocale(LC_ALL, NULL);
        if ( nullptr == c_all_tmp ) {
            throw ::cc::Exception("Unable to initialize C locale - nullptr- the request cannot be honored!");
        }
        const std::string lc_all = c_all_tmp;
        
        const char* c_numeric_tmp = setlocale(LC_NUMERIC, NULL);
        if ( nullptr ==  c_numeric_tmp ) {
            throw ::cc::Exception("Unable to initialize C numeric - nullptr- the request cannot be honored! ");
        }
        
        const std::string lc_numeric = c_numeric_tmp;

        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "LOCALE");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s - %s \n"     , "LC_ALL"    , lc_all.c_str(), "€ $ £");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s - " DOUBLE_FMT "\n", "LC_NUMERIC", lc_numeric.c_str(), (double)123.456);

        CC_IF_DEBUG(
            //
            // ... *printf function ...
            //
            if ( true == process_->is_master_ ) {
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ *%s\n", "printf(...)");
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "SIZET_FMT" , SIZET_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT8_FMT"  , INT8_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT8_FMT" , UINT8_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT16_FMT" , INT16_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT16_FMT", UINT16_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT32_FMT" , INT32_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT32_FMT", UINT32_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "INT64_FMT" , INT64_FMT);
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "UINT64_FMT", UINT64_FMT);
            }
        );
        
        //
        // ... ICU / V8 ...
        //
                
        // ... OPENSSL ...
        if ( nullptr == strnstr(process_->name_.c_str(), "nginx-", sizeof(char) * 6) ) {
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
            OPENSSL_config(NULL);
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
#else
            if ( 0 == OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL) ) {
                throw ::cc::Exception("Unable to initialize openssl!");
            }
            /*  OPENSSL_init_ssl() may leave errors in the error queue while returning success */
            ERR_clear_error();
#endif
        } // else { /* nginx will take care of openssl initialization */ }
        
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "OPENSSL");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION"  , SSLeay_version(SSLEAY_VERSION));
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "FLAGS"    , SSLeay_version(SSLEAY_CFLAGS));
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "BUILT ON" , SSLeay_version(SSLEAY_BUILT_ON));
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "PLATFORM" , SSLeay_version(SSLEAY_PLATFORM));
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "DIR"      , SSLeay_version(SSLEAY_DIR));

        // ... LIBEVENT2 ...
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "LIBEVENT2");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", event_get_version());
        const int evthread_use_pthreads_rv = evthread_use_pthreads();
        if ( 0 == evthread_use_pthreads_rv ) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "PTHREADS", "OK");
        } else {
            throw ::cc::Exception("Unable to initialize libevent2, error code is %d", (int)evthread_use_pthreads_rv);
        }

    // ... ICU ...
    #ifdef __APPLE__
        const std::string icu_dat_file_uri = ::cc::fs::Dir::Normalize(directories_->share_) + CC_IF_DEBUG_ELSE("icu/debug/icudtl.dat", "icu/icudtl.dat");
    #else
        const std::string icu_dat_file_uri = ::cc::fs::Dir::Normalize(directories_->share_) + "icu/icudtl.dat";
    #endif

    #ifndef CASPER_REQUIRE_GOOGLE_V8

        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "ICU");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", U_ICU_VERSION);
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "DATA FILE", icu_dat_file_uri.c_str());
        const UErrorCode icu_error_code = ::cc::icu::Initializer::GetInstance().Load(icu_dat_file_uri);
        if ( UErrorCode::U_ZERO_ERROR == icu_error_code ) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " OK\n", "INIT");
        } else {
            throw ::cc::Exception("Unable to initialize ICU, error code is %d : %s", (int)icu_error_code,
                                  ::cc::icu::Initializer::GetInstance().load_error_msg().c_str()
            );
        }

        if ( true == a_v8.required_ ) {
            throw ::cc::Exception("V8 is required but initializer was not compiled with V8 support!");
        }
        
    #else

        // ... V8 ...
        if ( true == a_v8.required_ ) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "V8");
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", ::v8::V8::GetVersion());
            cc::v8::Singleton::GetInstance().Startup(/* a_exec_uri     */ sys::Process::GetExecURI(process_->pid_).c_str(),
                                                     /* a_icu_data_uri */ icu_dat_file_uri.c_str()
            );
        }
        if ( true == a_v8.runs_on_main_thread_ ) {
            ::cc::v8::Singleton::GetInstance().Initialize();
        }
        
        // ... ICU ...
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "ICU");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", U_ICU_VERSION);
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "DATA FILE", icu_dat_file_uri.c_str());

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
            
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "LOCALE", icu_default_locale.getBaseName());
        
        //
        // .. cURL ...
        //
        
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "cURL");
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", "VERSION", curl_version());
        const CURLcode curl_init_rv = cc::curl::Initializer::GetInstance().Start();
        if ( CURLE_OK == curl_init_rv ) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " OK\n", "INIT");
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
        
        //
        // ... present ...
        //
        if ( nullptr != a_present ) {
            std::string                        title;
            std::map<std::string, std::string> values;
            a_present(title, values);
            if ( values.size() > 0 ) {
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", title.c_str());
                for ( const auto& p : values ) {
                    CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %s\n", p.first.c_str(), p.second.c_str());
                }
            }
        }

        //
        // ... warm-up signals ...
        //
        ::ev::Signals::GetInstance().WarmUp(*loggable_data_);
        // ... tips ...
        if ( true == process_->is_master_ && true == being_debugged_ ) {
            const std::string kill_cmd_prefix = std::string(0 == getuid() ? "sudo " : "" );
            const std::string kill_cmd_suffix = std::to_string(process_->pid_);
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "SIGNALS");
            for ( auto signal : ::ev::Signals::GetInstance().Supported() ) {
                CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " %-65.65s: %s\n",
                                          signal.name_.c_str(),
                                          signal.purpose_.c_str(),
                                          ( kill_cmd_prefix + "kill -`kill -l " + std::string(signal.name_.c_str()) + "` " +  kill_cmd_suffix ).c_str()
                );
            }
        }
                
        // ... done ..
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* %s - %s process w/pid %u configured...\n",
            process_->info_.c_str(), ( true == process_->is_master_ ? "master" : "worker" ), process_->pid_
        );

    } catch (const std::overflow_error& a_of_e) {
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* std::overflow_error: %s\n", a_of_e.what());
        exit(-1);
    } catch (const std::bad_alloc& a_bad_alloc) {
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* std::bad_alloc: %s\n", a_bad_alloc.what());
        exit(-1);
    } catch (const std::runtime_error& a_rt_e) {
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* std::runtime_error: %s\n", a_rt_e.what());
        exit(-1);
    } catch (const std::exception& a_std_exception) {
        CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* std::exception: %s\n", a_std_exception.what());
        exit(-1);
    } catch (...) {
        try {
            ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
        } catch (::cc::Exception& a_cc_exception) {
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\n* %s\n", a_cc_exception.what());
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
    
    CC_GLOBAL_INITIALIZER_LOG("cc-status","* %s - %s process w/pid %d is starting up...\n",
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
    ::ev::Signals::GetInstance().Startup(a_signals.register_,
            /* a_callbacks */
            {
                /* on_signal_           */ a_signals.unhandled_signals_callback_,
                /* on_fatal_exception_  */ a_callbacks.on_fatal_exception_,
                /* call_on_main_thread_ */ a_callbacks.call_on_main_thread_
            }
    );

    // ... mark as initialized ...
    initialized_ = true;

    CC_GLOBAL_INITIALIZER_LOG("cc-status","* %s - %s process w/pid %d is started up...\n",
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
    const std::string log_line_prefix = (
        process_->info_ + " - " +  ( true == process_->is_master_ ? "master" : "worker" ) + " process w/pid " + std::to_string(process_->pid_) + " is"
    );
    
    CC_GLOBAL_INITIALIZER_LOG("cc-status","* %s %s...\n",
                              log_line_prefix.c_str(), true == a_for_cleanup_only ? "cleaning" : "shutting down"
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
        
        CC_GLOBAL_INITIALIZER_LOG("cc-status","* %s %s...\n",
                                  log_line_prefix.c_str(), "going down"
        );

        // ... debug trace ...
        osal::debug::Trace::Destroy();

        // .... signals ...
        ::ev::Signals::Destroy();
    } else {
        CC_GLOBAL_INITIALIZER_LOG("cc-status","* %s %s...\n",
                                  log_line_prefix.c_str(), "cleaned up"
        );
    }

    // ... mark as NOT warmed-up ...
    warmed_up_ = false;
    
    // ... mark as NOT initialized ...
    initialized_ = false;
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
            CC_GLOBAL_INITIALIZER_LOG("cc-status","\n\t⌥ %s\n", "LOGS");
        }
        
        // ... all conditions are met to register token ...
        const std::string uri = ( entry.uri_.length() > 0 ? entry.uri_ : ( directories_->log_ + entry.token_ + ".log" ) ) ;

        CC_GLOBAL_INITIALIZER_LOG("cc-status","\t\t- " CC_GLOBAL_INITIALIZER_KEY_FMT " [" UINT8_FMT "] %s\n", entry.token_.c_str(), entry.version_, uri.c_str());
        
        switch(entry.version_) {
            case 0:
                ::osal::debug::Trace::GetInstance().Register(entry.token_, stdout);
                break;
            case 1:
                ::ev::Logger::GetInstance().Register(entry.token_, uri);
                break;
            case 2:
                ::ev::LoggerV2::GetInstance().cc::logs::Logger::Register(entry.token_, uri);
                break;
            default:
                throw ::cc::Exception("Unsupported logger version %d", entry.version_);
        }
    }
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
