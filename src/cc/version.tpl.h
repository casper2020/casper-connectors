/**
 * @file version.h
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
#ifndef CASPER_CONNECTORS_VERSION_H_
#define CASPER_CONNECTORS_VERSION_H_

#ifndef CASPER_CONNECTORS_ABBR
#define CASPER_CONNECTORS_ABBR "cc"
#endif

#ifndef CASPER_CONNECTORS_NAME
#define CASPER_CONNECTORS_NAME "casper-connectors"
#endif

#ifndef CASPER_CONNECTORS_VERSION
#define CASPER_CONNECTORS_VERSION "x.x.x"
#endif

#ifndef CASPER_CONNECTORS_REL_NAME
#define CASPER_CONNECTORS_REL_NAME "n.n.n"
#endif

#ifndef CASPER_CONNECTORS_REL_DATE
#define CASPER_CONNECTORS_REL_DATE "r.r.d"
#endif

#ifndef CASPER_CONNECTORS_REL_BRANCH
#define CASPER_CONNECTORS_REL_BRANCH "r.r.b"
#endif

#ifndef CASPER_CONNECTORS_REL_HASH
#define CASPER_CONNECTORS_REL_HASH "r.r.h"
#endif

#ifndef CASPER_CONNECTORS_REL_TARGET
#define CASPER_CONNECTORS_REL_TARGET "r.r.t"
#endif

#ifndef CASPER_CONNECTORS_INFO
#define CASPER_CONNECTORS_INFO CASPER_CONNECTORS_NAME " v" CASPER_CONNECTORS_VERSION
#endif

#define CASPER_CONNECTORS_BANNER \
"  ____    _    ____  ____  _____ ____        ____ ___  _   _ _   _ _____ ____ _____ ___  ____  ____  " \
" / ___|  / \\  / ___||  _ \\| ____|  _ \\      / ___/ _ \\| \\ | | \\ | | ____/ ___|_   _/ _ \\|  _ \\/ ___| " \
"| |     / _ \\ \\___ \\| |_) |  _| | |_) |____| |  | | | |  \\| |  \\| |  _|| |     | || | | | |_) \\___ \\ " \
"| |___ / ___ \\ ___) |  __/| |___|  _ <_____| |__| |_| | |\\  | |\\  | |__| |___  | || |_| |  _ < ___) |" \
" \\____/_/   \\_\\____/|_|   |_____|_| \\_\\     \\____\\___/|_| \\_|_| \\_|_____\\____| |_| \\___/|_| \\_\\____/ "

#endif // CASPER_CONNECTORS_VERSION_H_
