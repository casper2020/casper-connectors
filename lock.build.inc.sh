#!/bin/bash
#
# Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
#
# casper-connectors is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# casper-connectors  is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
#

#
# Dependencies:
#
#  - [ SYSTEM / BINARIES] - libpq
#  - [ SYSTEM / BINARIES] - https://github.com/libevent/libevent
#
#  - [ SOURCE ] 	  - https://github.com/redis/hiredis
#  - [ SOURCE ] 	  - git@github.com:inoce1e/osal.git
#

source "../casper-packager/common/lock.build.inc.sh"
if [ $? -ne 0 ]; then
    exit -1
fi

#############################################
# NRS OSAL
#############################################
# NRS_OSAL_REPO_LOCK=""

#############################################
# HIREDIS
#############################################
# HIREDIS_REPO_LOCK=""

#############################################
# REQUIRED REPOS
#############################################
REQUIRED_REPOS=("." "${NRS_OSAL_REPO_LOCAL_DIR}" "${HIREDIS_LOCAL_DIR}")
