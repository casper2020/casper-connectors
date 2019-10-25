/**
 * @file types.h
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
#ifndef NRS_CC_DEBUG_TYPES_H_
#define NRS_CC_DEBUG_TYPES_H_

#if !defined(NDEBUG) || defined(DEBUG) || defined(_DEBUG) || defined(ENABLE_DEBUG)
    #define CC_DEBUG_ON 1
#else
    #define CC_DEBUG_ON
#endif

#ifdef CC_DEBUG_ON

    #define CC_IF_DEBUG(a_code) a_code
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_debug

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name) a_type a_name
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value) a_type a_name = a_value

#else

    #define CC_IF_DEBUG(a_code)
    #define CC_IF_DEBUG_ELSE(a_debug, a_release) a_release

    #define CC_IF_DEBUG_DECLARE_VAR(a_type, a_name)
    #define CC_IF_DEBUG_DECLARE_AND_SET_VAR(a_type, a_name, a_value)

#endif

#endif // NRS_CC_DEBUG_TYPES_H_
