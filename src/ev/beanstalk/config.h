/**
 * @file types.h
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
#ifndef NRS_EV_BEANSTALK_TYPES_H_
#define NRS_EV_BEANSTALK_TYPES_H_

#include <string> // std::string
#include <set>    // std::set

namespace ev
{
    
    namespace beanstalk
    {
  
        typedef struct _Config {
            
            std::string           host_;             //!< host
            int                   port_;             //!< port number
            float                 timeout_;          //!< in seconds
            uint32_t              abort_polling_;    //!< in seconds
            std::set<std::string> tubes_;            //!< tubes
            std::set<std::string> sessionless_tubes_;
            std::set<std::string> action_tubes_;
            
            inline void operator=(const _Config& a_config)
            {
                host_          = a_config.host_;
                port_          = a_config.port_;
                timeout_       = a_config.timeout_;
                abort_polling_ = a_config.abort_polling_;
                for ( auto it : a_config.tubes_ ) {
                    tubes_.insert(it);
                }
                for ( auto it : sessionless_tubes_ ) {
                    sessionless_tubes_.insert(it);
                }
                for ( auto tube : a_config.action_tubes_ ) {
                    action_tubes_.insert(tube);
                }
            }
            
        } Config;

    } // end of namespace 'beanstalk'
    
} // end of namespace 'ev'

#endif // NRS_EV_BEANSTALK_TYPES_H_
