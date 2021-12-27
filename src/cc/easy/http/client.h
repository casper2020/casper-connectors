/**
 * @file client.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_EASY_HTTP_CLIENT_H_
#define NRS_CC_EASY_HTTP_CLIENT_H_

#include "cc/easy/http/base.h"

namespace cc
{

    namespace easy
    {

        namespace http
        {
            
            class Client final : public ::cc::easy::http::Base, private ::ev::scheduler::Client
            {

            public: // Constructor / Destructor

                Client () = delete;
                Client (const ev::Loggable::Data& a_loggable_data, const char* const a_user_agent = nullptr);
                virtual ~Client();

            protected: // Inherited Virtual Method(s) / Function(s) - Implementation // Overload

                virtual void Async (const Method a_method,
                                    const std::string& a_url, const Headers& a_headers, const std::string* a_body,
                                    Callbacks a_callbacks, const Timeouts* a_timeouts);

            }; // end of class 'Client'
            
        } // end of namespace 'http'

    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_HTTP_CLIENT_H_
