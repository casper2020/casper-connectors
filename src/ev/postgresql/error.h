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
#ifndef NRS_EV_POSTGRESQL_ERROR_H_
#define NRS_EV_POSTGRESQL_ERROR_H_

#include "ev/error.h"

namespace ev
{
    
    namespace postgresql
    {

        class Error final : public ::ev::Error
        {
            
        public: // Constructor(s) / Destructor
            
            Error(const std::string& a_message);
            Error(const char* const a_format, ...) __attribute__((format(printf, 2, 3)));
            virtual ~Error ();
            
        }; // end of class 'Error'

    } // end of namespace 'postgresql'
    
} // end of namespace error

#endif // NRS_EV_POSTGRESQL_ERROR_H_
