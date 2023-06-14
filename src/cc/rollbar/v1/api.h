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

#pragma once
#ifndef CC_ROLLBAR_V1_API_H_
#define CC_ROLLBAR_V1_API_H_

#include "cc/non-movable.h"
#include "cc/non-copyable.h"

#include "cc/easy/http/client.h"

#include "ev/loggable.h"

#include "json/json.h"

namespace cc
{

    namespace rollbar
    {

        namespace v1
        {
            
            class API final : public ::cc::NonMovable, public ::cc::NonCopyable
            {
                
            public: // Data Type(s)
                
                typedef struct {
                    const std::string name_;
                    const std::string version_;
                } Notifier;
                
            private: // Const Data
                
                const Notifier notifier_;
                
            private: // Data
                
                Json::Value             config_;
                
            private: // Helper(s)
                
                cc::easy::http::Client* client_;
                
            public: // Constructor(s) / Destructor
                
                API() = delete;
                API(const Notifier notifier_);
                virtual ~API();
            
            public: // One-Shot Call Method(s)  / Function(s)
                
                void Setup (const ev::Loggable::Data& a_loggable_data, const Json::Value& a_config);
                
            public: // Method(s)  / Function(s)
                
                void Create (const std::string& a_level, const std::string& a_title, const std::string& a_message,
                             const Json::Value& a_custom = Json::Value::null);
                
            }; // end of class 'API'
            
        } // end of namespace 'v1'
        
    } // end of namespace 'rollbar'
    
} // end of namespace 'cc'

#endif // CC_ROLLBAR_V1_API_H_
