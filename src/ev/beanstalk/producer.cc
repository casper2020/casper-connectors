/**
 * @file producer.cc
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

#include "ev/beanstalk/producer.h"

#include "ev/exception.h"

/**
 * @brief Default constructor.
 *
 * @param a_config
 */
ev::beanstalk::Producer::Producer (const ev::beanstalk::Config& a_config)
{
    client_ = new Beanstalk::Client(a_config.host_, a_config.port_, a_config.timeout_);
    if ( a_config.tubes_.size() == 0 ) {
        throw ev::Exception("Producer does not have tubes!");
    } else if ( a_config.tubes_.size() > 1) {
        throw ev::Exception("Producer does not support multiple tubes!");
    }
    tube_ = ( *a_config.tubes_.begin() );
    if ( false == client_->use(tube_) ) {
        throw ev::Exception("Unable to assign beanstalk tube named '%s'!", tube_.c_str());
    }
    if ( 0 != strcasecmp(tube_.c_str(), "default") ) {
        (void)client_->ignore("default");
    }
}

/**
 * @brief Constructor with 'tube' override.
 *
 * @param a_config
 * @param a_tube
 */
ev::beanstalk::Producer::Producer (const ev::beanstalk::Config& a_config, const std::string& a_tube)
{
    client_ = new Beanstalk::Client(a_config.host_, a_config.port_, a_config.timeout_);
    tube_   = a_tube;
    if ( false == client_->use(tube_) ) {
        throw ev::Exception("Unable to assign beanstalk tube named '%s'!", tube_.c_str());
    }
    if ( 0 != strcasecmp(tube_.c_str(), "default") ) {
        (void)client_->ignore("default");
    }
}

/**
 * @brief Destructor.
 */
ev::beanstalk::Producer::~Producer ()
{
    delete client_;
}
