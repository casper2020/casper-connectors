/**
 * @file signals.c
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
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

#include "ev/signals.h"

#include "cc/logs/basic.h"

#include "cc/debug/types.h"

#include "ev/logger.h"
#include "ev/logger_v2.h"

#include <signal.h>

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Signal handler.
 *
 * @param a_sig_no
 */
static void ev_sa_handler (int a_sig_no)
{
    (void)ev::Signals::GetInstance().OnSignal(a_sig_no);
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 *
 * @param a_instance A referece to the owner of this class.
*/
ev::Initializer::Initializer (ev::Signals& a_instance)
 : cc::Initializer<ev::Signals>(a_instance)
{
    instance_.loggable_data_ = nullptr;
    instance_.logger_client_ = nullptr;
    instance_.callbacks_     = { nullptr, nullptr };
}

/**
 * @brief Destructor.
*/
ev::Initializer::~Initializer ()
{
    if ( nullptr != instance_.loggable_data_ ) {
        delete instance_.loggable_data_;
    }
    if ( nullptr != instance_.logger_client_ ) {
        delete instance_.logger_client_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Warm-up singleton.
 *
 * @param a_loggable_data_ref
 *
 */
void ev::Signals::WarmUp (const ev::Loggable::Data& a_loggable_data_ref)
{
    if ( nullptr != logger_client_ ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__ );
    }
    
    loggable_data_ = new ev::Loggable::Data(a_loggable_data_ref);
    logger_client_ = new ev::LoggerV2::Client(*loggable_data_);
    
    logger_client_->Unset(LoggerV2::Client::LoggableFlags::IPAddress | LoggerV2::Client::LoggableFlags::OwnerPTR);
    loggable_data_->Update(loggable_data_->module(), loggable_data_->ip_addr(), __FUNCTION__);

    ev::LoggerV2::GetInstance().Register(logger_client_, { "signals"} );
    ev::LoggerV2::GetInstance().Log(logger_client_, "signals", "--- WARM-UP ---");
    
    /* id_ */  /* name_ */  /* description_ */  /* purpose_ */
    supported_ = {
        { SIGQUIT, "SIGQUIT", strsignal(SIGQUIT), "Quit application." },
        { SIGTERM, "SIGTERM", strsignal(SIGTERM), "Terminate application." },
        { SIGTTIN, "SIGTTIN", strsignal(SIGTTIN), "PostgreSQL Connections Invalidation && Gatekeeper Configs Reload." },
        { SIGUSR1, "SIGUSR1", strsignal(SIGUSR1), "Logs Recycling." },
        { SIGUSR2, "SIGUSR2", strsignal(SIGUSR2), "Soft shutdown." }
    };
}

/**
 * @brief Prepare singleton, register signals to listen to and a callback for unhandled signals.
 *
 * @param a_signals   A set of signals to handle.
 * @param a_callbacks A set of functions to be called accordingly to status.
 */
void ev::Signals::Startup (const std::set<int>& a_signals,
                           Callbacks a_callbacks)
{
    
    if ( true == ev::scheduler::Scheduler::GetInstance().IsRegistered(this) ) {
        throw ::cc::Exception("Logic error - %s already called!", __PRETTY_FUNCTION__ );
    }
    
    ev::LoggerV2::GetInstance().Log(logger_client_, "signals", "--- STARTUP ---");
    
    loggable_data_->Update(loggable_data_->module(), loggable_data_->ip_addr(), __FUNCTION__);
    
    struct sigaction act;

    memset(&act, 0, sizeof(act));

    sigemptyset(&act.sa_mask);
    act.sa_handler = ev_sa_handler;
    act.sa_flags   = SA_NODEFER;

    for ( auto signal : a_signals ) {
        sigaction(signal, &act, 0);
        signals_.insert(signal);
    }
    
    callbacks_ = a_callbacks;
    
    ev::scheduler::Scheduler::GetInstance().Register(this);
}

/**
 * @brief Cleanup previous singleton settings.
 */
void ev::Signals::Shutdown ()
{
    if ( nullptr == logger_client_ ) {
        return;
    }

    loggable_data_->Update(loggable_data_->module(), loggable_data_->ip_addr(), __FUNCTION__);
    
    ev::LoggerV2::GetInstance().Log(logger_client_, "signals", "--- SHUTDOWN ---");

    struct sigaction act;

    memset(&act, 0, sizeof(act));

    act.sa_handler = SIG_DFL;

    for ( auto signal : signals_ ) {
        sigaction(signal, &act, 0);
    }
    signals_.clear();

    callbacks_ = { nullptr, nullptr };
    
    ev::LoggerV2::GetInstance().Unregister(logger_client_);
    
    delete logger_client_;
    logger_client_ = nullptr;
    
    delete loggable_data_;
    loggable_data_ = nullptr;
}

/**
 * @brief Append more signal handlers.
 *
 * @param a_handlers Hnadler information.
 */
void ev::Signals::Append (const std::vector<ev::Signals::Handler>& a_handlers)
{
    std::vector<::ev::Signals::Handler>* vector = nullptr;
    for ( auto handler : a_handlers ) {
        auto it = other_signal_handlers_.find(handler.signal_);
        if ( other_signal_handlers_.end() == it ) {
            vector = new std::vector<::ev::Signals::Handler>();
            other_signal_handlers_[handler.signal_] = vector;
        } else {
            vector = it->second;
        }
        vector->push_back(ev::Signals::Handler(handler));
    }
}

/**
 * @brief
 */
void ev::Signals::Unregister ()
{
    ev::scheduler::Scheduler::GetInstance().Unregister(this);
    for ( auto it : other_signal_handlers_ ) {
        it.second->clear();
        delete it.second;
    }
    other_signal_handlers_.clear();
    callbacks_ = { nullptr, nullptr };
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Call this function when a signal was intercepted.
 *
 * @param a_sig_no
 *
 * @return True if sinal was handled, false otherwise.
 */
bool ev::Signals::OnSignal (const int a_sig_no)
{
    bool rv = false;

    // ... if not ready to log ...
    if ( loggable_data_ == nullptr ) {
      // ... not ready to handle signals ...
      return rv;
    }

    loggable_data_->Update(loggable_data_->module(), loggable_data_->ip_addr(), __FUNCTION__);

    const char* const name = strsignal(a_sig_no);

    ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                    "Signal %s received...",
                                    ( nullptr != name ? name : std::to_string(a_sig_no).c_str() )
    );
    
    // ... handle signal ...
    switch(a_sig_no) {
        case SIGUSR1:
        {
            ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                          "Signal %2d - Recycle logs.",
                                          a_sig_no
            );
            ::cc::logs::Basic::GetInstance().Recycle();
            ::ev::Logger::GetInstance().Recycle();
            ::ev::LoggerV2::GetInstance().Recycle();
            CC_DEBUG_LOG_RECYCLE();
            rv = true;
        }
            break;
        case SIGTTIN:
        {
            if ( nullptr != callbacks_.call_on_main_thread_ ) {
                ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                              "Signal %2d - Scheduling PostgreSQL connection(s) invalidation...",
                                              SIGTTIN
                );
                callbacks_.call_on_main_thread_([this](){
                    NewTask([this] () -> ::ev::Object* {
                        ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                                      "Signal %2d - Invalidate PostgreSQL connection(s)...",
                                                      SIGTTIN
                        );
                        return new:: ev::Request(*loggable_data_, ev::Request::Target::PostgreSQL, ev::Request::Mode::OneShot, ev::Request::Control::Invalidate);
                    })->Finally([this] (::ev::Object* /* a_object */) {
                        ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                                      "Signal %d - PostgreSQL connection(s) invalidated.",
                                                      SIGTTIN
                        );
                    })->Catch([this] (const ::ev::Exception& a_ev_exception) {
                        ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                                      "Signal %2d - Unable to invalidate PostgreSQL connections: '%s'",
                                                      SIGTTIN, a_ev_exception.what()
                        );
                    });
                    }
                );
            } else {
                ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                              "Signal %2d - PostgreSQL connection(s) invalidation REJECTED - not ready yet!",
                                              SIGTTIN
                );
            }
            rv = true;
        }
            break;
        case SIGUSR2:
        case SIGQUIT:
        case SIGTERM:
        {
            ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                            "Signal %2d - %s shutdown %s special handling.",
                                            a_sig_no, SIGUSR2 == a_sig_no ? "Soft" : "Hard" , nullptr != callbacks_.on_signal_ ? "with" : "without"
            );
        }
        default:
            if ( nullptr != callbacks_.on_signal_ ) {
                rv = callbacks_.on_signal_(a_sig_no);
                if ( true == rv ) {
                    return rv;
                }
            }
            break;
    }
    
    // ... notify other signal handlers ...
    for ( auto it : other_signal_handlers_ ) {
        for ( auto handler : (*it.second) ) {
            if ( it.first != a_sig_no ) {
                continue;;
            }
            ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                          "Signal %2d - %s...",
                                            a_sig_no, handler.description_.c_str()
            );
            try {
                const std::string msg = handler.callback_();
                ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                              "Signal %2d - %s...",
                                                a_sig_no, msg.c_str()
                );
            } catch (const ::cc::Exception& a_cc_exception) {
                ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                                "Signal %2d - %s",
                                                a_sig_no, a_cc_exception.what()
                );
            } catch (...) {
                try {
                    ::cc::Exception::Rethrow(/* a_unhandled */ true, __FILE__, __LINE__, __FUNCTION__);
                } catch (::cc::Exception& a_cc_exception) {
                    ev::LoggerV2::GetInstance().Log(logger_client_, "signals",
                                                    "Signal %2d - %s",
                                                    a_sig_no, a_cc_exception.what()
                    );
                }
            }
        }
    }
    
    return rv;
}

/**
 * @brief Create a new task.
 *
 * @param a_callback The first callback to be performed.
 */
ev::scheduler::Task* ev::Signals::NewTask (const EV_TASK_PARAMS& a_callback)
{
    return new ev::scheduler::Task(a_callback,
                                   [this](::ev::scheduler::Task* a_task) {
                                       ev::scheduler::Scheduler::GetInstance().Push(this, a_task);
                                   }
    );
}

