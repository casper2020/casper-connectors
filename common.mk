#
# Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
#
# This file is part of casper-connectors.
#
# casper-connectors is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# casper-connectors is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
#

PLATFORM=$(shell uname -s)

####################
# Set library type
####################
ifndef LIB_TYPE
  LIB_TYPE:="static"
endif
override LIB_TYPE:=$(shell echo $(LIB_TYPE) | tr A-Z a-z)

# validate library type
ifeq (static, $(LIB_TYPE))
#
else
$(error "Don't know how to build for target $(LIB_TYPE) ")
endif

####################
# Set target type
####################
ifeq (Darwin, $(PLATFORM))
 ifndef TARGET
   TARGET:=Debug
 else
   override TARGET:=$(shell echo $(TARGET) | tr A-Z a-z)
   $(eval TARGET_E:=$$$(TARGET))
   TARGET_E:=$(shell echo $(TARGET_E) | tr A-Z a-z )
   TARGET_S:=$(subst $(TARGET_E),,$(TARGET))
   TARGET_S:=$(shell echo $(TARGET_S) | tr a-z A-Z )
   TMP_TARGET:=$(TARGET_S)$(TARGET_E)
   override TARGET:=$(TMP_TARGET)
 endif
 OUT_DIR = ./out/darwin
else
 ifndef TARGET
   TARGET:=debug
 else
   override TARGET:=$(shell echo $(TARGET) | tr A-Z a-z)
 endif
 OUT_DIR = ./out/linux
endif
TARGET_LC:=$(shell echo $(TARGET) | tr A-Z a-z)

# validate target
ifeq (release, $(TARGET_LC))
  #
else
  ifeq (debug, $(TARGET_LC))
    #
  else
    $(error "Don't know how to build for target $(TARGET_LC) ")
  endif
endif

OUT_DIR_FOR_TARGET = $(OUT_DIR)/$(TARGET)

# postgresql
ifndef PG_CONFIG
  ifeq (Darwin, $(PLATFORM))
    PG_CONFIG:= ../casper-packager/postgresql/darwin/pkg/$(TARGET_LC)/11.4/usr/local/casper/postgresql/bin/pg_config
  else
    PG_CONFIG:= ../casper-packager/postgresql/linux/pkg/$(TARGET_LC)/11.4/usr/local/casper/postgresql/bin/pg_config
  endif
endif
POSTGRESQL_HEADERS_DIR         := $(shell $(PG_CONFIG) --includedir)
POSTGRESQL_HEADERS_SERVER_DIR  := $(shell $(PG_CONFIG) --includedir-server)
POSTGRESQL_HEADERS_OTHER_C_DIR := $(shell $(PG_CONFIG) --pkgincludedir)

# libevent, openssl and cURL
ifeq (Darwin, $(PLATFORM))
  LIBEVENT2_HEADERS_DIR := ../casper-packager/libevent2/darwin/pkg/${TARGET_LC}/libevent2/usr/local/casper/libevent2/include
  OPENSSL_HEADERS_DIR := ../casper-packager/openssl/darwin/pkg/${TARGET_LC}/openssl/usr/local/casper/openssl/include
  CURL_HEADERS_DIR := ../casper-packager/curl/darwin/pkg/$(TARGET)/curl/usr/local/casper/curl/include
  LIBUNWIND_HEADERS_DIR := ../casper-packager/libunwind/darwin/pkg/$(TARGET)/libunwind/usr/local/casper/libunwind/include
else
  LIBEVENT2_HEADERS_DIR := ../casper-packager/libevent2/linux/pkg/${TARGET_LC}/libevent2/usr/local/casper/libevent2/include
  OPENSSL_HEADERS_DIR   := ../casper-packager/openssl/linux/pkg/${TARGET_LC}/openssl/usr/local/casper/openssl/include
  CURL_HEADERS_DIR     := ../casper-packager/curl/linux/pkg/$(TARGET)/curl/usr/local/casper/curl/include
  LIBUNWIND_HEADERS_DIR := ../casper-packager/libunwind/linux/pkg/$(TARGET)/libunwind/usr/local/casper/libunwind/include
endif

EV_SRC :=                           \
									./src/ev/logger_v2.cc                                                         \
									./src/ev/signals.cc                                                           \
									./src/ev/device.cc                                                            \
									./src/ev/error.cc                                                             \
									./src/ev/hub/handler.cc                                                       \
									./src/ev/hub/hub.cc                                                           \
									./src/ev/hub/keep_alive_handler.cc                                            \
									./src/ev/hub/one_shot_handler.cc                                              \
									./src/ev/object.cc                                                            \
									./src/ev/postgresql/device.cc                                                 \
									./src/ev/postgresql/error.cc                                                  \
									./src/ev/postgresql/json_api.cc                                               \
									./src/ev/postgresql/object.cc                                                 \
									./src/ev/postgresql/reply.cc                                                  \
									./src/ev/postgresql/request.cc                                                \
									./src/ev/postgresql/value.cc                                                  \
									./src/ev/redis/device.cc                                                      \
									./src/ev/redis/error.cc                                                       \
									./src/ev/redis/object.cc                                                      \
									./src/ev/redis/reply.cc                                                       \
									./src/ev/redis/request.cc                                                     \
									./src/ev/redis/subscriptions/manager.cc                                       \
									./src/ev/redis/subscriptions/reply.cc                                         \
									./src/ev/redis/subscriptions/request.cc                                       \
									./src/ev/redis/value.cc                                                       \
									./src/ev/redis/session.cc                                                     \
									./src/ev/curl/device.cc                                                       \
									./src/ev/curl/error.cc                                                        \
									./src/ev/curl/http.cc                                                         \
									./src/ev/curl/object.cc                                                       \
									./src/ev/curl/reply.cc                                                        \
									./src/ev/curl/request.cc                                                      \
									./src/ev/curl/value.cc                                                        \
									./src/ev/request.cc                                                           \
									./src/ev/result.cc                                                            \
									./src/ev/scheduler/object.cc                                                  \
									./src/ev/scheduler/scheduler.cc                                               \
									./src/ev/scheduler/subscription.cc                                            \
									./src/ev/scheduler/task.cc                                                    \
									./src/ev/scheduler/unique_id_generator.cc                                     \
									./src/ev/beanstalk/consumer.cc                                                \
									./src/ev/beanstalk/producer.cc                                                \
									./src/ev/auth/route/gatekeeper.cc                                             \
									./src/cc/errors/jsonapi/tracker.cc                                            \
									./src/cc/errors/tracker.cc                                                    \
									./src/cc/backtrace/unwind.cc                                                  \
									./src/cc/fs/file.cc                                                           \
									./src/cc/fs/posix/dir.cc                                                      \
									./src/cc/fs/posix/file.cc                                                     \
									./src/cc/fs/posix/xattr.cc                                                    \
									./src/cc/i18n/singleton.cc                                                    \
									./src/ev/casper/session.cc

AUTH_SRC := \
									./src/cc/auth/jwt.cc    \

HASH_SRC := \
                                                                        ./src/cc/hash/md5.cc

CRYPTO_SRC := \
									./src/cc/crypto/hmac.cc \
								        ./src/cc/crypto/rsa.cc

CC_SRC := \
                                      ./src/cc/utc_time.cc


HIREDIS_HEADERS_DIR :=              \
									../

BEANSTALK_CLIENT_HEADERS_DIR := \
                                   ../beanstalk-client

JSONCPP_HEADERS_DIR := ../jsoncpp/dist

CPPCODEC_HEADERS_DIR := ../cppcodec

ifndef ICU4C_INCLUDE_DIR
  ifeq (Darwin, $(PLATFORM))
    ICU4C_INCLUDE_DIR := /usr/local/opt/icu4c/include
  endif
endif

INCLUDE_DIRS :=                     \
									-I src                                                                       \
									-I ../casper-osal/src                                                               \
									-I $(HIREDIS_HEADERS_DIR)                                                    \
									-I $(JSONCPP_HEADERS_DIR)                                                    \
									-I $(POSTGRESQL_HEADERS_DIR)                                                 \
									-I $(POSTGRESQL_HEADERS_SERVER_DIR)                                          \
									-I $(POSTGRESQL_HEADERS_OTHER_C_DIR)                                         \
									-I $(BEANSTALK_CLIENT_HEADERS_DIR)                                           \
	                                -I $(CPPCODEC_HEADERS_DIR)

ifdef ICU4C_INCLUDE_DIR
  INCLUDE_DIRS += -I $(ICU4C_INCLUDE_DIR)
endif

ifeq (Darwin, $(PLATFORM))
  INCLUDE_DIRS += \
	         -I /usr/local/include
endif

# libevent, openssl and cURL
INCLUDE_DIRS += -I $(LIBEVENT2_HEADERS_DIR) -I $(OPENSSL_HEADERS_DIR) -I$(CURL_HEADERS_DIR) -I$(LIBUNWIND_HEADERS_DIR)

# ngx dependency?
ifdef NGX_DIR
	EV_SRC += src/ev/ngx/bridge.cc \
			  src/ev/ngx/shared_glue.cc
	INCLUDE_DIRS += -I $(NGX_DIR)/core -I $(NGX_DIR)/http -I $(NGX_DIR)/http/modules -I $(NGX_DIR)/http/v2 -I $(NGX_DIR)/event -I $(NGX_DIR)/os/unix -I $(NGX_DIR)/../objs/
endif

# compiler flags
C           := gcc
CXX         := g++
CXXFLAGS    := -std=gnu++11 $(INCLUDE_DIRS) -c -Wall
CFLAGS      := $(INCLUDE_DIRS) -c -Wall
RAGEL_FLAGS :=

ifeq ($(TARGET),release)
  CXXFLAGS += -g -O2 -DNDEBUG
else
  CXXFLAGS += -g -O0 -DDEBUG
endif

ifneq (static, $(LIB_TYPE))
$(error not ready to build $(LIB_TYPE) library)
else
  CFLAGS += -static
  CXXFLAGS += -static
endif

VERSION :=$(shell cat version)

# out dir
mk_out_dir:
	@mkdir -p $(OUT_DIR_FOR_TARGET)
	@echo "* [$(TARGET)] ${OUT_DIR_FOR_TARGET}..."

# c
%.c.o:
	@echo "* [$(TARGET)] c   $< ..."
	@$(C) $(CFLAGS) $< -o $@

# c++
.cc.o:
	@echo "* [$(TARGET)] cc  $< ..."
	@$(CXX) $(CXXFLAGS) $< -o $@

# c++
.cpp.o:
	@echo "* [$(TARGET)] cpp $< ..."
	@$(CXX) $(CXXFLAGS) $< -o $@

# clean
clean: clean_lib
	@echo "* [common-clean]..."
	@find . -name "*~" -delete
	@echo "* [common-clean] done..."
