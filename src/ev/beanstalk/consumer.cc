/**
 * @file consumer.cc
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

#include "ev/beanstalk/consumer.h"

#include "ev/exception.h"

#include <algorithm>
#include <limits>
#include <chrono>
#include <unistd.h> // usleep

/**
 * @brief Default constructor.
 */
ev::beanstalk::Consumer::Consumer ()
{
    client_ = nullptr;
}

/**
 * @brief Destructor.
 */
ev::beanstalk::Consumer::~Consumer ()
{
    if ( nullptr != client_ ) {
        delete client_;
    }
}

// MARK: -

/**
 * @brief Try to establish a beanstalkd connection.
 *
 * @param a_config    See \link ev::beanstalk::Config \link.
 * @param a_callbacks Set of callbacks to be used during connection attempts, see \link Consumer::ConnectCallbacks \link.
 * @param a_abort     Abort flag.
 */
void ev::beanstalk::Consumer::Connect (const ev::beanstalk::Config& a_config, const Consumer::ConnectCallbacks& a_callbacks, volatile bool& a_abort)
{
    const uint64_t          max_attempts = std::max(a_config.max_attempts_, static_cast<uint64_t>(1));
    const bool              unlimited    = ( std::numeric_limits<uint64_t>::max() == max_attempts ) ;
    const float             timeout      = std::max(a_config.timeout_, 3.0f);
    uint64_t                attempt      = 0;
    while ( a_abort == false && ( true == unlimited || ( unlimited == false && attempt < max_attempts ) ) ) {
        const auto start_tp = std::chrono::high_resolution_clock::now();
        try {
            a_callbacks.attempt_(++attempt, max_attempts, timeout);
            // ... try to connect ...
            client_ = new Beanstalk::Client(a_config.host_, a_config.port_, timeout);
            // ... if it reached here, it's connected ...
            for ( auto it : a_config.tubes_ ) {
                if ( it.length() > 0 && false == client_->watch(it) ) {
                    throw ev::Exception("Unable to assign beanstalk tube named '%s'!", it.c_str());
                }
            }
            (void)client_->ignore("default");
            // ... done ...
            break;
        } catch (const Beanstalk::ConnectException& a_c_e) {
            // ... notify ...
            a_callbacks.failure_(attempt, max_attempts, a_c_e.what());
            // ... if reached max attempts ...
            if ( attempt >= max_attempts ) {
                // ... rethrow ...
                throw a_c_e;
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_tp ).count();
        if ( static_cast<float>(elapsed) < ( timeout * 1000 ) ) {
            usleep(( ( timeout * 1000 ) - elapsed ) * 1000);
        }
    }
}
