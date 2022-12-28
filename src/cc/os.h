/**
 * @file os_defs.h
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
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_OS_DEFS_H_
#define NRS_CC_OS_DEFS_H_

#if defined(__APPLE__)
    #define CC_IF_DARWIN(...) ___VA_ARGS__
    #define CC_IF_LINUX(...)
#elif defined(linux) || defined(__linux) || defined(__linux__)
    #define CC_IF_DARWIN(...)
    #define CC_IF_LINUX(...) ___VA_ARGS__
#else
    #error ???
#endif

#endif // NRS_CC_OS_DEFS_H_
