/**
 * @file version.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_B64_H_
#define NRS_CC_B64_H_

#include "cc/pragmas.h"

CC_DIAGNOSTIC_PUSH()
CC_DIAGNOSTIC_IGNORED("-Wunused-parameter")
CC_DIAGNOSTIC_IGNORED("-Wsign-conversion")
#include "cppcodec/base64_url_unpadded.hpp"
#include "cppcodec/base64_rfc4648.hpp"
CC_DIAGNOSTIC_POP()

namespace cc
{

    typedef cppcodec::base64_url_unpadded base64_url_unpadded;
    typedef cppcodec::base64_rfc4648      base64_rfc4648;
    
} // end of namespace 'cc'

#endif // NRS_CC_B64_H_
