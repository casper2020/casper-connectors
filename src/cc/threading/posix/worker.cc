/**
 * @file worker.cc
 *
 * Copyright (c) 2011-2010 Cloudware S.A. All rights reserved.
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

#include "cc/threading/posix/worker.h"

#include "cc/exception.h"

#include <pthread.h> // pthread_setname_np
#include <signal.h>  // sigaddset

/**
 * @brief Static helper method to set current thread name.
 *
 * @param a_name Thead name to set, keep it short < 63 chars.
 */
void cc::threading::posix::Worker::SetName (const std::string& a_name)
{
    #ifdef __APPLE__
        pthread_setname_np(a_name.c_str());
    #else
        pthread_setname_np(pthread_self(), a_name.c_str());
    #endif
}

/**
 * @brief Static helper method to block current thead signals.
 *
 * @param a_signals Set of signals to block.
 *
 * @remarks sigemptyset and sigaddset are async-signal-safe according to the POSIX standard.
 */
void cc::threading::posix::Worker::BlockSignals (const std::set<int>& a_signals)
{
    sigset_t mask;
    sigemptyset (&mask);
    for ( auto signal : a_signals ) {
        sigaddset(&mask, signal);
    }
    const int rv = pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    if ( 0 != rv ) {
        throw cc::Exception("Unable to block thread signals: %d - %s!", rv, strerror(rv));
    }
}

