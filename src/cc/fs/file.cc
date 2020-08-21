/**
 * @file file.h
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/fs/file.h"

#ifdef __APPLE__
#pragma mark - Writer
#endif

/**
 * @brief Default constructor.
 */
cc::fs::file::Writer::Writer ()
{
   /* empty */
}

/**
 * @brief Destructor.
 */
cc::fs::file::Writer::~Writer ()
{
    /* empty */
}

#ifdef __APPLE__
#pragma mark - Reader
#endif

/**
 * @brief Default constructor.
 */
cc::fs::file::Reader::Reader ()
{
   /* empty */
}

/**
 * @brief Destructor.
 */
cc::fs::file::Reader::~Reader ()
{
    /* empty */
}
