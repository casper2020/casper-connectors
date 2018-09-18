/**
 * @file exception.h
 *
 * Copyright (c) 2010-2018 Neto Ranito & Seabra LDA. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CASPER_CONNECTORS_V8_EXCEPTION_H_
#define NRS_CASPER_CONNECTORS_V8_EXCEPTION_H_

#include "cc/exception.h"

namespace cc
{
    
    namespace v8
    {
        
        typedef ::cc::Exception Exception;
        
    } // end of namespace 'v8'

} // end of namespace 'cc'

#endif // NRS_CASPER_CONNECTORS_V8_EXCEPTION_H_
