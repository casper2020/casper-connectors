/**
 * @file includes.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifndef NRS_CASPER_CONNECTORS_V8_INCLUDES_H

#include "cc/pragmas.h"

CC_DIAGNOSTIC_PUSH()
CC_DIAGNOSTIC_IGNORED("-Wunused-parameter")
CC_DIAGNOSTIC_IGNORED("-Wsign-conversion")
CC_DIAGNOSTIC_IGNORED("-Wshadow-all")
CC_DIAGNOSTIC_IGNORED("-Wfloat-conversion")

#include "libplatform/libplatform.h"
#include "v8.h"
#include "v8-version.h"

CC_DIAGNOSTIC_POP()

#endif // NRS_CASPER_CONNECTORS_V8_INCLUDES_H
