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

#include "ev/logger.h"
#include "ev/logger_v2.h"

#include <signal.h>

/*
 * STATIC DATA INITIALIZATION
 */
const ev::Loggable::Data*                                ev::Signals::s_loggable_data_ptr_         = nullptr;

ev::Bridge*                                              ev::Signals::s_bridge_ptr_                = nullptr;
std::function<void(const ev::Exception& a_ev_exception)> ev::Signals::s_fatal_exception_callback_  = nullptr;
std::function<bool(int)>                                 ev::Signals::s_unhandled_signal_callback_ = nullptr;

/**
 * @brief Signal handler.
 *
 * @param a_sig_no
 */
static void ev_sa_handler (int a_sig_no)
{
    (void)ev::Signals::GetInstance().OnSignal(a_sig_no);
}


/**
 * @brief Prepare singleton.
 *
 * @param a_loggable_data_ref
 */
void ev::Signals::Startup (const ev::Loggable::Data& a_loggable_data_ref)
{
    s_loggable_data_ptr_ = &a_loggable_data_ref;
    ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                  "--- STARTUP ---"
    );
}

/**
 * @brief Register a handler for a set of signals.
 *
 * @param a_signals  A set of signals to handle.
 * @param a_callback A function to be called when a signal is not handled by this singleton.
 */
void ev::Signals::Register (const std::set<int>& a_signals, std::function<bool(int)> a_callback)
{
    struct sigaction act;
    
    memset(&act, 0, sizeof(act));
    
    act.sa_handler = ev_sa_handler;
    for ( auto signal : a_signals ) {
        sigaction(signal, &act, 0);
    }
    
    s_unhandled_signal_callback_ = a_callback;
}

/**
 * @brief Append a handler for a set of signals.
 *
 * @param a_signals  A set of signals to handle.
 * @param a_callback A function to be called when a signal is intercepted.
 */
void ev::Signals::Append (const std::set<int>& a_signals, std::function<void (int)> a_callback)
{
    std::vector<std::function<void(int)>>* vector = nullptr;
    for ( auto signal : a_signals ) {
        auto it = other_signal_handlers_.find(signal);
        if ( other_signal_handlers_.end() == it ) {
            vector = new std::vector<std::function<void(int)>>();
            other_signal_handlers_[signal] = vector;
        } else {
            vector = it->second;
        }
        vector->push_back(a_callback);
    }
}

/**
 * @brief
 *
 * @param a_bridge_ptr
 * @param a_fatal_exception_callback
 */
void ev::Signals::Register (Bridge* a_bridge_ptr, std::function<void(const ev::Exception&)> a_fatal_exception_callback)
{
    ev::scheduler::Scheduler::GetInstance().Register(this);
    s_bridge_ptr_               = a_bridge_ptr;
    s_fatal_exception_callback_ = a_fatal_exception_callback;
}

/**
 * @brief
 */
void ev::Signals::Unregister ()
{
    ev::scheduler::Scheduler::GetInstance().Unregister(this);
    s_bridge_ptr_               = nullptr;
    s_fatal_exception_callback_ = nullptr;    
    for ( auto it : other_signal_handlers_ ) {
        it.second->clear();
        delete it.second;
    }
    other_signal_handlers_.clear();
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
    ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                  "Signal %d Received...",
                                  a_sig_no
    );
    bool rv = false;
    
    // ... handle signal ...
    switch(a_sig_no) {
            
        case SIGUSR1:
        {
            ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                          "Signal %d - Recycle logs.",
                                          a_sig_no
            );
            ::ev::Logger::GetInstance().Recycle();
            ::ev::LoggerV2::GetInstance().Recycle();
            rv = true;
        }
            break;
            
        case SIGTTIN:
        {
            ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                          "Signal %d - Scheduling PostgreSQL connection(s) invalidation...",
                                          SIGHUP
            );
            if ( nullptr == s_bridge_ptr_ ) {
                ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                              "Signal %d - Signal ignored, at this moment, application is not ready or doesn't need to handle this signal right now.",
                                              SIGHUP
                );
                if ( nullptr != s_fatal_exception_callback_ ) {
                    s_fatal_exception_callback_(::ev::Exception("Application is not ready to handle signal SIGHUP!"));
                }
            } else {
                s_bridge_ptr_->CallOnMainThread([this](){
                    NewTask([this] () -> ::ev::Object* {
                        ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                                      "Signal %d - Invalidate PostgreSQL connection(s)...",
                                                      SIGHUP
                        );
                        return new:: ev::Request(*s_loggable_data_ptr_, ev::Request::Target::PostgreSQL, ev::Request::Mode::OneShot, ev::Request::Control::Invalidate);
                    })->Finally([this] (::ev::Object* /* a_object */) {
                        ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                                      "Signal %d - PostgreSQL connection(s) invalidated.",
                                                      SIGHUP
                        );
                    })->Catch([this] (const ::ev::Exception& a_ev_exception) {
                        ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                                      "Signal %d - Unable to invalidate PostgreSQL connections: '%s'",
                                                      SIGHUP, a_ev_exception.what()
                        );
                    });
                });
            }
            rv = true;
        }
            break;

        case SIGQUIT:
        case SIGTERM:
        {
            ev::Logger::GetInstance().Log("signals", *s_loggable_data_ptr_,
                                          "Signal %d - Clean shutdown.",
                                          a_sig_no
            );
        }
        default:
            rv = ( nullptr != s_unhandled_signal_callback_ ? s_unhandled_signal_callback_(a_sig_no) : false );
            break;
    }
    
    // ... notify other signal handlers ...
    for ( auto it : other_signal_handlers_ ) {
        for ( auto callback : (*it.second) ) {
            callback(it.first);
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

