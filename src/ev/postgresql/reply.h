/**
 * @file reply.h - PostgreSQL Reply
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
#ifndef NRS_EV_POSTGRESQL_REPLY_H_
#define NRS_EV_POSTGRESQL_REPLY_H_

#include "ev/postgresql/object.h"
#include "ev/postgresql/value.h"

namespace ev
{
    
    namespace postgresql
    {
        
        class Reply final : public ev::postgresql::Object
        {
            
        public: // Const Data
            
            const uint64_t elapsed_;
            
        protected: // Data
            
            Value value_;
            
        public: // Constructor(s) / Destructor
            
            Reply(PGresult* a_reply, const uint64_t a_elapsed);
            Reply(const ExecStatusType a_status, const char* const a_message, const uint64_t a_elapsed);
            virtual ~Reply();
            
        public: // Method(s) / Function(s)
            
            const Value& value () const;
        };
        
        /**
         * @return Read-only access to collected value.
         */
        inline const Value& Reply::value () const
        {
            return value_;
        }
        
    }
    
}

#endif // NRS_EV_POSTGRESQL_REPLY_H_
