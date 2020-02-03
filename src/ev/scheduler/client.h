/**
 * @file client.h
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
#pragma once
#ifndef NRS_EV_SCHEDULER_CLIENT_H_
#define NRS_EV_SCHEDULER_CLIENT_H_

#include <algorithm> // std::min

namespace ev
{
    
    namespace scheduler
    {
        
        class Client
        {
            
        private: // Data
            
            std::string id_;
            
        public: // Constructor(s) / Destructor
            
            /**
             * @brief Default constructor.
             */
            Client ()
            {
                id_ = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                id_ += "-";
                id_ += std::to_string(reinterpret_cast<uintptr_t>(this));
                id_ += "-";
                for (int idx = 0; idx < std::min(48, 63); ++idx ) {
                    id_ += alphanum()[random() % 62];
                }
                id_ += "-";
                id_ += std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            }
            
            /**
             * @brief Destructor.
             */
            virtual ~Client ()
            {
                /* empty */
            }
            
        public:
            
            const std::string& id () const;
            
        protected:
            
            /**
             * @return Const 64 length alpha numeric string.
             */
            constexpr const char* const alphanum () const
            {
                return "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
            }

        };
    
        /**
         * @return A previously random generated id.
         */
        inline const std::string& Client::id () const
        {
            return id_;
        }
            
    } // end of namespace 'scheduler'
    
} // end of namespace 'ev'

#endif // NRS_EV_SCHEDULER_CLIENT_H_
