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
#ifndef NRS_CC_TYPES_H_
#define NRS_CC_TYPES_H_

#include <inttypes.h>

#define INT8_FMT   "%" PRId8
#define UINT8_FMT  "%" PRIu8
#define UINT8_FMT_ZP(d) "%0" #d PRIu8

#define INT16_FMT  "%" PRId16
#define UINT16_FMT "%" PRIu16
#define UINT16_FMT_ZP(d) "%0" #d PRIu16

#define INT32_FMT  "%" PRId32
#define UINT32_FMT "%" PRIu32

#define INT64_FMT        "%" PRId64
#define INT64_FMT_ZP(d)  "%0" #d PRId64

#define INT64_FMT_MAX_RA "%19" PRId64

#define UINT64_FMT       "%" PRIu64
#define UINT64_FMT_ZP(d) "%0" #d PRIu64
#define UINT64_HEX_FMT   "%" PRIx64

#define LONG_FMT  "%" PRIdPTR
#define ULONG_FMT "%" PRIuPTR

#define LONG_FMT  "%" PRIdPTR
#define ULONG_FMT "%" PRIuPTR

#define LONG_LONG_FMT   "%" "lld"
#define U_LONG_LONG_FMT "%" "%llu"

#define DOUBLE_FMT "%lf"

#define SIZET_FMT "%zd"
#define SIZET_FMT_BP(d)   "%" #d "zu"
#define SSIZET_FMT_BP(d)  "%" #d "zd"

#endif // NRS_CC_TYPES_H_
