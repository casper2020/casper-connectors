/**
 * @file unique_id_generator.h
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
#ifndef NRS_EV_SCHEDULER_UNIQUE_ID_GENERATOR_H_
#define NRS_EV_SCHEDULER_UNIQUE_ID_GENERATOR_H_

#include "osal/osal_singleton.h"

#include <queue>      // std::queue
#include <inttypes.h> // uint64_t

namespace ev
{

    namespace scheduler
    {

        class UniqueIDGenerator : public osal::Singleton<UniqueIDGenerator>
        {
            
        public: // Const Data
            
            static uint64_t k_invalid_id_;
            
        private: // Data
            
            static uint64_t next_;
            
        protected: // Data
            
            std::queue<uint64_t> cached_;
            
        public: // Method(s) / Function(s)
            
            uint64_t Rent ();
            void     Return (uint64_t a_id);
            
        public: //
            
            void Startup  ();
            void Shutdown ();
            
        };// end of class 'UniqueIDGenerator'

    } // end of namespace 'scheduler'
    
} // end of namespace 'ev'

#endif // NRS_EV_SCHEDULER_UNIQUE_ID_GENERATOR_H_
