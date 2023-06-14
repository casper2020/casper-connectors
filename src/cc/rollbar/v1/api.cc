/**
 * @file api.h
 *
 * Copyright (c) 2017-2023 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/rollbar/v1/api.h"

#include "cc/easy/json.h"

/**
 * @brief Default constructor.
 *
 * @param a_notifier Notifier info.
 */
cc::rollbar::v1::API::API (const Notifier a_notifier)
: notifier_(a_notifier)
{
    client_ = nullptr;
}

/**
 * @brief Destructor.
 */
cc::rollbar::v1::API::~API ()
{
    if ( nullptr != client_ ) {
        delete client_;
    }
}

// MARK: -

void cc::rollbar::v1::API::Setup (const ev::Loggable::Data& a_loggable_data, const Json::Value& a_config)
{
    // ... sanity check ...
    CC_ASSERT(nullptr == client_);
    // ... setup HTTP client ...
    client_ = new cc::easy::http::Client(a_loggable_data);
    // ... save config ...
    config_ = a_config;
}

// MARK: -

/**
 * @brief Create a 'rollbar' item.
 */
void cc::rollbar::v1::API::Create (const std::string& a_level, const std::string& a_title, const std::string& a_message,
                                   const Json::Value& a_custom)
{
    const cc::easy::JSON<cc::Exception> json;
    // ... sanity check ...
    CC_ASSERT(nullptr != client_);
    // ... setup ...
    cc::easy::http::Client::Headers headers = {
        { "content-type", { "application/json" } },
        { "accept",       { "application/json" } },
    };
    // ... set headers ...
    const Json::Value& headers_ref = json.Get(config_, "headers", Json::ValueType::objectValue, /* a_default */ nullptr);
    for ( const auto& member : headers_ref.getMemberNames() ) {
        headers[member].push_back(json.Get(headers_ref, member.c_str(), Json::ValueType::stringValue, /* a_default */ nullptr).asString());
    }
    //
    // https://docs.rollbar.com/reference/create-item
    //
    // At the time of writing:
    //
    // {
    //   "data": {
    //     "environment": "production",
    //     "body": {
    //       // Option 3: "message"
    //       // Only one of "trace", "trace_chain", "message", or "crash_report" should be present.
    //       // Presence of a "message" key means that this payload is a log message.
    //       "message": {
    //         // Required: body
    //         // The primary message text, as a string
    //         "body": "Request over threshold of 10 seconds",
    //       },
    //     },
    //     // Optional: level
    //     // The severity level. One of: "critical", "error", "warning", "info", "debug"
    //     "level": "error",
    //     // Optional: timestamp
    //     // When this occurred, as a unix timestamp.
    //     "timestamp": 1369188822,
    //     // Optional: platform
    //     // The platform on which this occurred. Meaningful platform names:
    //     // "browser", "android", "ios", "flash", "client", "heroku", "google-app-engine"
    //     "platform": "linux",
    //     // Optional: framework
    //     "framework": "pyramid",
    //     // Optional: custom
    //     // Any arbitrary metadata you want to send. "custom" itself should be an object.
    //     "custom": {},
    //     // Optional: title
    //     // A string that will be used as the title of the Item occurrences will be grouped into.
    //     // Max length 255 characters.
    //     // If omitted, we'll determine this on the backend.
    //     "title": "NameError when setting last project in views/project.py",
    //     // Optional: notifier
    //     // Describes the library used to send this event.
    //     "notifier": {
    //       // Optional: name
    //       // Name of the library
    //       "name": "pyrollbar",
    //       // Optional: version
    //       // Library version string
    //       "version": "0.5.5"
    //     }
    //   }
    // }
    // ... set body ...

    Json::Value body = Json::Value(Json::ValueType::objectValue);
    body["data"]["body"]["message"]["body"] = a_message;
    body["data"]["environment"] = json.Get(config_, "environment", Json::ValueType::stringValue, /* a_default */ nullptr);
    body["data"]["level"]       = a_level;
    body["data"]["timestamp"]   = std::to_string(time(nullptr));
    body["data"]["title"]       = a_title;
    // ... platform ...
#ifdef __APPLE__
    body["data"]["platform"] = "macOS";
#else // assuming linux
    body["data"]["platform"] = "linux";
#endif
    body["data"]["framework"] = "casper";
    // ... notifier ...
    body["data"]["notifier"]["name"]    = notifier_.name_;
    body["data"]["notifier"]["version"] = notifier_.version_;
    // ... custom data ...
    if ( false == a_custom.isNull() ) {
        body["data"]["custom"] = a_custom;
    }
    // ... perform ...
    const Json::Value& url = json.Get(config_, "url", Json::ValueType::stringValue, /* a_default */ nullptr);
    client_->POST(url.asString(), headers, json.Write(body),
                  /* a_callbacks */ {
                        /* on_success_ */ [] (const ::ev::curl::Value&) {
                        },
                        /* on_error_ */ [] (const ::ev::curl::Error&) {
                        },
                        /* on_exception_ */ [] (const cc::Exception&) {
                        }
                  }
    );
}
