/**
 * @file error.h
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
#ifndef NRS_EV_ERROR_H_
#define NRS_EV_ERROR_H_

#include "ev/object.h"

#include <stdint.h>
#include <inttypes.h>

#include <string>

namespace ev
{
    
    class Error : public Object
    {
        
    protected: // Data
        
        std::string message_;
        
    public: // Constructor(s) / Destructor

        Error(const Target a_target, const std::string& a_message);
        virtual ~Error ();
        
    public: // Method(s) / Function(s)
        
        const std::string& message () const;
        
    }; // end of class 'Error'
    
    inline const std::string& Error::message () const
    {
        return message_;
    }

} // end of namespace error

#endif // NRS_EV_ERROR_H_
