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

/**
 * @brief Default constructor.
 *
 * @param a_config
 */
ev::beanstalk::Consumer::Consumer (const ev::beanstalk::Config& a_config)
{
    client_ = new Beanstalk::Client(a_config.host_, a_config.port_, a_config.timeout_);
    for ( auto it : a_config.tubes_ ) {
        if ( it.length() > 0 && false == client_->watch(it) ) {
            throw ev::Exception("Unable to assign beanstalk tube named '%s'!", it.c_str());
        }
    }
    (void)client_->ignore("default");
}

/**
 * @brief Destructor.
 */
ev::beanstalk::Consumer::~Consumer ()
{
    delete client_;
}
