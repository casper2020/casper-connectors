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
# casper-connectors  is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
#

include common.mk

OBJECTS := \
           $(EV_SRC:.cc=.o)        \
           $(CC_SRC:.cc=.o)        \
           $(CRYPTO_SRC:.cc=.o)    \
           $(HASH_SRC:.cc=.o)      \
           $(AUTH_SRC:.cc=.o)

LIB_NAME := libconnectors.a
A_FILE := $(OUT_DIR_FOR_TARGET)/$(LIB_NAME)

all: mk_out_dir $(OBJECTS)
	@ar rcs $(A_FILE) $(OBJECTS)
	@echo "* [$(TARGET)] $(A_FILE) ~> done"

clean_lib:
	@echo "* [clean] $(LIB_NAME)..."
	@rm -f $(OBJECTS)
	@rm -f $(A_FILE)

.SECONDARY:
