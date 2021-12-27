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
# casper-connectors is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
#

############################
# CONNECTORS VARIABLES
############################

CONNECTORS_CC_FS_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/fs/file.cc       \
  $(PROJECT_SRC_DIR)/src/cc/fs/posix/dir.cc  \
  $(PROJECT_SRC_DIR)/src/cc/fs/posix/file.cc \
  $(PROJECT_SRC_DIR)/src/cc/fs/posix/xattr.cc

CONNECTORS_CC_THREADING_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/threading/posix/worker.cc

CONNECTORS_CC_I18N_SRC:= \
  $(PROJECT_SRC_DIR)/src/cc/i18n/singleton.cc

CONNECTORS_CC_ERRORS_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/errors/tracker.cc         \
  $(PROJECT_SRC_DIR)/src/cc/errors/jsonapi/tracker.cc

CONNECTORS_CC_AUTH_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/auth/jwt.cc \

CONNECTORS_CC_HASH_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/hash/md5.cc \
  $(PROJECT_SRC_DIR)/src/cc/hash/sha256.cc

CONNECTORS_CC_CRYPTO_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/crypto/hmac.cc \
  $(PROJECT_SRC_DIR)/src/cc/crypto/rsa.cc

CONNECTORS_CC_CRT_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/crt/x509_reader.cc

CONNECTORS_CC_EASY_SRC:= \
  $(PROJECT_SRC_DIR)/src/cc/easy/beanstalk.cc          \
  $(PROJECT_SRC_DIR)/src/cc/easy/redis.cc              \
  $(PROJECT_SRC_DIR)/src/cc/easy/http.cc               \
  $(PROJECT_SRC_DIR)/src/cc/easy/http/base.cc          \
  $(PROJECT_SRC_DIR)/src/cc/easy/http/oauth2/client.cc \
  $(PROJECT_SRC_DIR)/src/cc/easy/http/client.cc        \
  $(PROJECT_SRC_DIR)/src/cc/easy/job/job.cc            \
  $(PROJECT_SRC_DIR)/src/cc/easy/job/handler.cc

CONNECTORS_CC_DEBUG_SRC:= \
 $(PROJECT_SRC_DIR)/src/cc/debug/logger.cc \
 $(PROJECT_SRC_DIR)/src/cc/debug/types.cc

CONNECTORS_CC_V8_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/v8/script.cc          \
  $(PROJECT_SRC_DIR)/src/cc/v8/context.cc         \
  $(PROJECT_SRC_DIR)/src/cc/v8/singleton.cc       \
  $(PROJECT_SRC_DIR)/src/cc/v8/basic/evaluator.cc

CONNECTORS_CC_LOGS_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/logs/basic.cc

CONNECTORS_CC_SYS_SRC := \
  $(PROJECT_SRC_DIR)/src/sys/process.cc

ifeq (Darwin, $(PLATFORM))
CONNECTORS_CC_SYS_SRC += \
  $(PROJECT_SRC_DIR)/src/sys/bsd/process.cc
endif

CONNECTORS_CC_GLOBAL_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/global/initializer.cc

CONNECTORS_CC_ICU_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/icu/initializer.cc

CONNECTORS_CC_CURL_SRC := \
  $(PROJECT_SRC_DIR)/src/cc/curl/initializer.cc

CONNECTORS_CC_SRC := \
  $(CONNECTORS_CC_LOGS_SRC)             \
  $(CONNECTORS_CC_GLOBAL_SRC)           \
  $(CONNECTORS_CC_THREADING_SRC)        \
  $(PROJECT_SRC_DIR)/src/cc/utc_time.cc \
  $(PROJECT_SRC_DIR)/src/cc/optarg.cc   \
  $(CONNECTORS_CC_FS_SRC)               \
  $(CONNECTORS_CC_I18N_SRC)             \
  $(CONNECTORS_CC_ERRORS_SRC)           \
  $(CONNECTORS_CC_EASY_SRC)             \
  $(CONNECTORS_CC_DEBUG_SRC)

CONNECTORS_EV_LOOP_SRC := \
 $(PROJECT_SRC_DIR)/src/ev/loop/bridge.cc            \
 $(PROJECT_SRC_DIR)/src/ev/loop/beanstalkd/object.cc \
 $(PROJECT_SRC_DIR)/src/ev/loop/beanstalkd/runner.cc \
 $(PROJECT_SRC_DIR)/src/ev/loop/beanstalkd/looper.cc \
 $(PROJECT_SRC_DIR)/src/ev/loop/beanstalkd/job.cc

CONNECTORS_EV_SRC := \
  $(PROJECT_SRC_DIR)/src/ev/logger_v2.cc                     \
  $(PROJECT_SRC_DIR)/src/ev/signals.cc                       \
  $(PROJECT_SRC_DIR)/src/ev/device.cc                        \
  $(PROJECT_SRC_DIR)/src/ev/error.cc                         \
  $(PROJECT_SRC_DIR)/src/ev/hub/handler.cc                   \
  $(PROJECT_SRC_DIR)/src/ev/hub/hub.cc                       \
  $(PROJECT_SRC_DIR)/src/ev/hub/keep_alive_handler.cc        \
  $(PROJECT_SRC_DIR)/src/ev/hub/one_shot_handler.cc          \
  $(PROJECT_SRC_DIR)/src/ev/object.cc                        \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/device.cc             \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/error.cc              \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/json_api.cc           \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/object.cc             \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/reply.cc              \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/request.cc            \
  $(PROJECT_SRC_DIR)/src/ev/postgresql/value.cc              \
  $(PROJECT_SRC_DIR)/src/ev/redis/device.cc                  \
  $(PROJECT_SRC_DIR)/src/ev/redis/error.cc                   \
  $(PROJECT_SRC_DIR)/src/ev/redis/object.cc                  \
  $(PROJECT_SRC_DIR)/src/ev/redis/reply.cc                   \
  $(PROJECT_SRC_DIR)/src/ev/redis/request.cc                 \
  $(PROJECT_SRC_DIR)/src/ev/redis/subscriptions/manager.cc   \
  $(PROJECT_SRC_DIR)/src/ev/redis/subscriptions/reply.cc     \
  $(PROJECT_SRC_DIR)/src/ev/redis/subscriptions/request.cc   \
  $(PROJECT_SRC_DIR)/src/ev/redis/value.cc                   \
  $(PROJECT_SRC_DIR)/src/ev/redis/session.cc                 \
  $(PROJECT_SRC_DIR)/src/ev/curl/device.cc                   \
  $(PROJECT_SRC_DIR)/src/ev/curl/error.cc                    \
  $(PROJECT_SRC_DIR)/src/ev/curl/http.cc                     \
  $(PROJECT_SRC_DIR)/src/ev/curl/object.cc                   \
  $(PROJECT_SRC_DIR)/src/ev/curl/reply.cc                    \
  $(PROJECT_SRC_DIR)/src/ev/curl/request.cc                  \
  $(PROJECT_SRC_DIR)/src/ev/curl/value.cc                    \
  $(PROJECT_SRC_DIR)/src/ev/request.cc                       \
  $(PROJECT_SRC_DIR)/src/ev/result.cc                        \
  $(PROJECT_SRC_DIR)/src/ev/scheduler/object.cc              \
  $(PROJECT_SRC_DIR)/src/ev/scheduler/scheduler.cc           \
  $(PROJECT_SRC_DIR)/src/ev/scheduler/subscription.cc        \
  $(PROJECT_SRC_DIR)/src/ev/scheduler/task.cc                \
  $(PROJECT_SRC_DIR)/src/ev/scheduler/unique_id_generator.cc \
  $(PROJECT_SRC_DIR)/src/ev/beanstalk/consumer.cc            \
  $(PROJECT_SRC_DIR)/src/ev/beanstalk/producer.cc            \
  $(PROJECT_SRC_DIR)/src/ev/auth/route/gatekeeper.cc         \
  $(PROJECT_SRC_DIR)/src/ev/casper/session.cc

# ngx dependency?
ifdef NGX_DIR
  CONNECTORS_EV_SRC += \
    $(PROJECT_SRC_DIR)/src/ev/ngx/bridge.cc \
    $(PROJECT_SRC_DIR)/src/ev/ngx/shared_glue.cc
endif

CONNECTORS_INCLUDE_DIRS := \
  -I $(PROJECT_SRC_DIR)/src

# ngx dependency?
ifdef NGX_DIR
  CONNECTORS_INCLUDE_DIRS += -I $(NGX_DIR)/core -I $(NGX_DIR)/http -I $(NGX_DIR)/http/modules -I $(NGX_DIR)/http/v2 -I $(NGX_DIR)/event -I $(NGX_DIR)/os/unix -I $(NGX_DIR)/../objs/
endif

# dependencies
CONNECTORS_DEPENDENCIES := \
  libevent2-dep-on \
  openssl-dep-on icu-dep-on curl-dep-on postgresql-dep-on zlib-dep-on \
  casper-osal-dep-on lemon-dep-on jsoncpp-dep-on \
  cppcodec-dep-on \
  beanstalk-client-dep-on hiredis-dep-on

ifeq (true, $(V8_DEP_ON))
  CONNECTORS_DEPENDENCIES += v8-dep-on 
endif

set-dependencies: $(CONNECTORS_DEPENDENCIES)
  ifeq (true, $(ICU_DEP_ON))
    CONNECTORS_CC_SRC += $(CONNECTORS_CC_ICU_SRC)
  else ifeq (true, $(ICU_STAND_ALONE_DEP_ON))
    CONNECTORS_CC_SRC += $(CONNECTORS_CC_ICU_SRC)
  endif
  ifeq (true, $(CURL_DEP_ON))
    CONNECTORS_CC_SRC += $(CONNECTORS_CC_CURL_SRC)
  endif
  ifeq (true, $(V8_DEP_ON))
    CONNECTORS_CC_SRC += $(CONNECTORS_CC_V8_SRC)
  endif
  ifeq (true, $(EV_DEP_ON))
    CONNECTORS_EV_SRC += $(CONNECTORS_EV_LOOP_SRC)
  endif
  CC_SRC := \
    $(CONNECTORS_EV_SRC)        \
    $(CONNECTORS_CC_SRC)        \
    $(CONNECTORS_CC_CRYPTO_SRC) \
    $(CONNECTORS_CC_CRT_SRC)    \
    $(CONNECTORS_CC_HASH_SRC)   \
    $(CONNECTORS_CC_AUTH_SRC)   \
    $(CONNECTORS_CC_SYS_SRC)
