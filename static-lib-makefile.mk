#
# Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
#
# This file is part of casper-connectors.
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

THIS_DIR := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ifeq (casper-connectors, $(shell basename $(THIS_DIR)))
  PACKAGER_DIR := $(abspath $(THIS_DIR)/../casper-packager)
else
  PACKAGER_DIR := $(abspath $(THIS_DIR)/..)
endif

include $(PACKAGER_DIR)/common/c++/settings.mk

PROJECT_SRC_DIR     := $(ROOT_DIR)/casper-connectors
EXECUTABLE_NAME     := 
EXECUTABLE_MAIN_SRC :=
LIBRARY_TYPE        := static
ifeq (true, $(V8_DEP_ON))
  LIBRARY_NAME := libconnectors-v8.a
else
  LIBRARY_NAME := libconnectors.a
endif
VERSION             := $(shell cat $(PROJECT_SRC_DIR)/VERSION)
CHILD_CWD           := $(THIS_DIR)
CHILD_MAKEFILE      := $(firstword $(MAKEFILE_LIST))

############################                                                                                                                                                                                                                  
# CONNECTORS VARIABLES                                                                                                                                                                                                                              
############################    

include $(PROJECT_SRC_DIR)/common.mk

############################
# COMMON REQUIRED VARIABLES
############################
INCLUDE_DIRS := \
  $(CONNECTORS_INCLUDE_DIRS)

CC_SRC := \
  $(CONNECTORS_EV_SRC)        \
  $(CONNECTORS_CC_SRC)        \
  $(CONNECTORS_CC_CRYPTO_SRC) \
  $(CONNECTORS_CC_HASH_SRC)   \
  $(CONNECTORS_CC_AUTH_SRC)

OBJECTS := \
  $(CC_SRC:.cc=.o)

include $(PACKAGER_DIR)/common/c++/common.mk

set-dependencies: icu-dep-on

all: lib

.SECONDARY:


# include common.mk

# EXECUTABLE_NAME     :=
# EXECUTABLE_MAIN_SRC :=
# LIBRARY_TYPE        := static
# ifeq (true, $(V8_DEP_ON))
#   LIBRARY_NAME := libconnectors-v8.a
# else
#   LIBRARY_NAME := libconnectors.a
# endif
# CHILD_CWD           := $(CURDIR)
# CHILD_MAKEFILE      := $(MAKEFILE_LIST)

# include ../casper-packager/common/c++/common.mk

# all: lib

# .SECONDARY:
