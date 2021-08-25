/**
* @file beanstalk.cc
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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

#include "cc/easy/beanstalk.h"

#include "cc/exception.h"

#include "json/json.h"

/**
 * @brief Default constructor.
 *
 * @param a_mode Operation mode, one of \link cc::easy::Beanstalk::Mode \link
 */
cc::easy::Beanstalk::Beanstalk (const Mode& a_mode)
    : mode_(a_mode)
{
    producer_ = nullptr;
}

/**
 * @brief Destructor.
 */
cc::easy::Beanstalk::~Beanstalk ()
{
    if ( nullptr != producer_ ) {
        delete producer_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Connect to a BEANSTALKD instance.
 *
 * @param a_ip     Host ip address.
 * @param a_port   Host port number.
 * @param a_tubes  Tubes to use.
 * @param a_timeout Connection timeout, 0 if none.
*/
void cc::easy::Beanstalk::Connect (const std::string& a_ip, const int a_port, const std::set<std::string>& a_tubes, const float a_timeout)
{
    // ... first release connection ( if any ) ...
    Disconnect();
    try {
        // ... estabilish new connection ...
        if ( ::cc::easy::Beanstalk::Mode::Producer == mode_ ) {
            const ::ev::beanstalk::Config config = {
                /* host_              */ a_ip,
                /* port_              */ a_port,
                /* timeout_           */ a_timeout,
                /* abort_polling_     */ 0,
                /* max_attempts_      */ std::numeric_limits<uint64_t>::max(),
                /* tubes_             */ a_tubes,
                /* sessionless_tubes_ */ { },
                /* action_tubes_      */ { },
            };
            producer_ = new ::ev::beanstalk::Producer(config);
        } else {
            throw ::cc::Exception("Mode " UINT8_FMT " not supported or implemented yet!", static_cast<uint8_t>(mode_));
        }
    } catch (const ::Beanstalk::ConnectException& a_btc_exception) {
        throw cc::Exception("BEANSTALK CONNECTOR: %s", a_btc_exception.what());
    }
}

/**
 * @brief Disconnect from a BEANSTALKD instance.
 */
void cc::easy::Beanstalk::Disconnect ()
{
    if ( ::cc::easy::Beanstalk::Mode::Producer == mode_ ) {
        if ( nullptr != producer_ ) {
            delete producer_;
            producer_ = nullptr;
        }
    } else {
        throw ::cc::Exception("Mode " UINT8_FMT " not supported or implemented yet!", static_cast<uint8_t>(mode_));
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Push a Job to the currently connected BEANSTALKD instance / tube.
 *
 * @param a_id       Job ID.
 * @param a_ttr      Job TTR ( time to run ) in seconds.
 * @param a_validity Job validity, in seconds.
*/
void cc::easy::Beanstalk::Push (const std::string& a_id, const size_t& a_ttr, const size_t& a_validity)
{
    Push (a_id, "", {}, a_ttr, a_validity);
}

/**
 * @brief Push a Job to the currently connected BEANSTALKD instance / tube.
 *
 * @param a_id       Job ID.
 * @param a_args     Job body arguments.
 * @param a_ttr      Job TTR ( time to run ) in seconds.
 * @param a_validity Job validity, in seconds.
*/
void cc::easy::Beanstalk::Push (const std::string& a_id, const std::map<std::string, std::string>& a_args, const size_t& a_ttr, const size_t& a_validity)
{
    Push (a_id, "", a_args, a_ttr, a_validity);
}

/**
 * @brief Push a Job to the currently connected BEANSTALKD instance / tube.
 *
 * @param a_id       Job ID.
 * @param a_payload  Job body payload ( JSON string ).
 * @param a_ttr      Job TTR ( time to run ) in seconds.
 * @param a_validity Job validity, in seconds.
*/
void cc::easy::Beanstalk::Push (const std::string& a_id, const std::string& a_payload, const size_t& a_ttr, const size_t& a_validity)
{
    Push (a_id, a_payload, {}, a_ttr, a_validity);
}

/**
 * @brief Push a Job to the currently connected BEANSTALKD instance / tube.
 *
 * @param a_id       Job ID.
 * @param a_payload  Job body payload ( JSON string ).
 * @param a_args     Job body arguments.
 * @param a_ttr      Job TTR ( time to run ) in seconds.
 * @param a_validity Job validity, in seconds.
*/
void cc::easy::Beanstalk::Push (const std::string& a_id, const std::string& a_payload, const std::map<std::string, std::string>& a_args, const size_t& a_ttr, const size_t& a_validity)
{
    // ... ensure a valid connection is already established ...
    EnsureConnection("push job");
    
    // ... parse payload ( if any ) ...
    Json::Value payload;

    if ( a_payload.length() > 0 ) {
        Json::Reader reader;
        // ... parse payload as JSON ...
        if ( false == reader.parse(a_payload, payload) ) {
            // ... an error occurred ...
            const auto errors = reader.getStructuredErrors();
            if ( errors.size() > 0 ) {
                throw cc::Exception("An error ocurred while parsing gatekeeper configuration: %s!",
                                      reader.getFormatedErrorMessages().c_str()
                );
            } else {
                throw cc::Exception("An error ocurred while parsing gatekeeper configuration!");
            }
        }
    } else {
            payload = Json::Value(Json::ValueType::objectValue);
    }
    // ... set or override parameters ...
    payload["id"]       = a_id;
    payload["tube"]     = producer_->tube();
    payload["validity"] = static_cast<Json::UInt64>(a_validity);
    // ... override with extra keys ...
    for ( auto p : a_args ) {
        payload[p.first] = p.second;
    }
    // ... serialize JSON and send to beanstalkd ...
    Json::FastWriter writer;
    const int64_t status = producer_->Put(writer.write(payload),
                                          /* a_priority = 0 */ 0,
                                          /* a_delay = 0 */ 0,
                                          static_cast<uint32_t>(a_ttr)
    );
    if ( status < 0 ) {
        throw cc::Exception("Beanstalk client returned with error code " + std::to_string(status) + ", while adding job '"
                              +
                            payload["id"].asString() + "' to '" + payload["tube"].asString() + "' tube!"
        );
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Ensure we've a valid connection to BEANSTALKD.
 *
 * @param a_action The action that request this validation.
 */
void cc::easy::Beanstalk::EnsureConnection (const char* const a_action)
{
    if ( ::cc::easy::Beanstalk::Mode::Producer == mode_ ) {
        if ( nullptr == producer_ ) {
            throw ::cc::Exception("Could't %s - no connection established.", a_action);
        }
    } else {
        throw ::cc::Exception("Mode " UINT8_FMT " not supported or implemented yet!", static_cast<uint8_t>(mode_));
    }
}
