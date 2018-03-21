/**
 * @file includes.h
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

#ifndef NRS_EV_REDIS_INCLUDES_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wconversion"
#ifdef __APPLE__
    #pragma GCC diagnostic ignored "-Wshorten-64-to-32"
    #pragma GCC diagnostic ignored "-Wc99-extensions"
#endif
extern "C" {
    #pragma GCC system_header
    #include "hiredis/hiredis.h"
    #include "hiredis/async.h"
    #include "hiredis/adapters/libevent.h"
    #include "hiredis/sds.h"
}
#pragma GCC diagnostic pop

#endif // NRS_EV_REDIS_INCLUDES_H_
