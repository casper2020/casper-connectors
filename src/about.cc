/**
 * @file about.cc
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

#include "about.h"
#include "version.h"

const char* const casper::connectors::ABBR ()
{
    return CASPER_CONNECTORS_ABBR;
}

const char* const casper::connectors::NAME ()
{
    return CASPER_CONNECTORS_NAME;
}

const char* const casper::connectors::VERSION ()
{
    return CASPER_CONNECTORS_VERSION;
}

const char* const casper::connectors::REL_DATE ()
{
    return CASPER_CONNECTORS_REL_DATE;
}

const char* const casper::connectors::REL_BRANCH ()
{
    return CASPER_CONNECTORS_REL_BRANCH;
}

const char* const casper::connectors::REL_HASH ()
{
    return CASPER_CONNECTORS_REL_HASH;
}

const char* const casper::connectors::INFO ()
{
    return CASPER_CONNECTORS_INFO;
}
