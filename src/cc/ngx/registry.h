/**
 * @file registry.h
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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
#ifndef NRS_CC_NGX_REGISTRY_H_
#define NRS_CC_NGX_REGISTRY_H_

#include "cc/singleton.h"

#include "ev/ngx/includes.h"

#include <map>
#include <mutex>

namespace cc
{

    namespace ngx
    {

        // ---- //
        class Registry;
        class RegistryInitializer final : public ::cc::Initializer<Registry>
        {
            
        public: // Constructor(s) / Destructor
            
            RegistryInitializer (Registry& a_instance);
            virtual ~RegistryInitializer ();
            
        }; // end of class 'RegistryInitializer'

        // ---- //
        class Registry final :  public cc::Singleton<Registry, RegistryInitializer>
        {
            
            friend class RegistryInitializer;
        
        private: // Threading
            
            std::mutex mutex_;
            
        private: // Data
            
            std::map<const ngx_event_t*, const void*> events_;
            
        public: // Method(s) / Function(s)
            
            void Register   (const ngx_event_t*, const void* a_data);
            void Unregister (const ngx_event_t*);
            
            const void* Data (const ngx_event_t* a_event);
            
        }; // end of class 'Registry'

    } // end of namespace 'ngx'

}  // end of namespace 'cc'

#endif // NRS_CC_NGX_REGISTRY_H_

