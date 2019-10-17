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

################
# CC SRC
################

CC_FS_SRC := \
	./src/cc/fs/file.cc                                                           \
	./src/cc/fs/posix/dir.cc                                                      \
	./src/cc/fs/posix/file.cc                                                     \
	./src/cc/fs/posix/xattr.cc

CC_I18N_SRC:= \
	./src/cc/i18n/singleton.cc

CC_ERRORS_SRC := \
	./src/cc/errors/tracker.cc         \
	./src/cc/errors/jsonapi/tracker.cc

CC_AUTH_SRC := \
	./src/cc/auth/jwt.cc    \

CC_HASH_SRC := \
	./src/cc/hash/md5.cc

CC_CRYPTO_SRC := \
	./src/cc/crypto/hmac.cc \
	./src/cc/crypto/rsa.cc

CC_V8_SRC := \
 ./src/cc/v8/script.cc  \
 ./src/cc/v8/context.cc \
 ./src/cc/v8/singleton.cc

CC_SRC := \
	./src/cc/utc_time.cc \
	$(CC_FS_SRC)     \
	$(CC_I18N_SRC)   \
	$(CC_ERRORS_SRC) \

ifeq (true, $(V8_DEP_ON))
  CC_SRC += $(CC_V8_SRC)
endif

EV_LOOP_SRC := \
 src/ev/loop/bridge.cc            \
 src/ev/loop/beanstalkd/runner.cc \
 src/ev/loop/beanstalkd/looper.cc \
 src/ev/loop/beanstalkd/job.cc

EV_SRC := \
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
	./src/ev/casper/session.cc

ifeq (true, $(V8_DEP_ON))
  EV_SRC += $(EV_LOOP_SRC)
endif

# ngx dependency?
ifdef NGX_DIR
  EV_SRC += \
	src/ev/ngx/bridge.cc \
	src/ev/ngx/shared_glue.cc
endif

################
# INCLUDE_DIRS
################

INCLUDE_DIRS = \
	-I src

# ngx dependency?
ifdef NGX_DIR
  INCLUDE_DIRS += -I $(NGX_DIR)/core -I $(NGX_DIR)/http -I $(NGX_DIR)/http/modules -I $(NGX_DIR)/http/v2 -I $(NGX_DIR)/event -I $(NGX_DIR)/os/unix -I $(NGX_DIR)/../objs/
endif

################
# OBJECTS
################
OBJECTS := \
           $(EV_SRC:.cc=.o)        \
           $(CC_SRC:.cc=.o)        \
           $(CC_CRYPTO_SRC:.cc=.o)    \
           $(CC_HASH_SRC:.cc=.o)      \
           $(CC_AUTH_SRC:.cc=.o)

# dependencies
CONNECTORS_DEPENDENCIES := casper-osal-dep-on jsoncpp-dep-on hiredis-dep-on beanstalk-client-dep-on zlib-dep-on icu-dep-on curl-dep-on cppcodec-dep-on postgresql-dep-on
ifeq (true, $(V8_DEP_ON))
  CONNECTORS_DEPENDENCIES += v8-dep-on 
endif

set-dependencies: $(CONNECTORS_DEPENDENCIES)

