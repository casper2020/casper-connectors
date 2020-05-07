/**
 * @file macros.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of nginx-broker.
 *
 * nginx-broker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nginx-broker  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with nginx-broker.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_MACROS_H_
#define NRS_CC_MACROS_H_

#define CC_DO_PRAGMA(x) _Pragma (#x)
#define CC_MACRO_DEFER(M,...) M(__VA_ARGS__)
#define CC_MACRO_STRINGIFY_ARG(a) #a

#define CC_SILENCE_UNUSED_VARIABLE(a_name) \
    (void)a_name

#define CC_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

#define CC_WARNING_UNUSED_VARIABLE(a_name) \
  _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("TODO 2.0: unused variable '" #a_name "'"))); \
  (void)a_name;

#define CC_WARNING_TODO(a_name) \
   _Pragma(CC_MACRO_STRINGIFY_ARG(GCC warning("" #a_name)))

#define CC_MARK_INTENDED_VIRTUAL_OVERRIDING(function) \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Woverloaded-virtual\"") \
    function \
    _Pragma("clang diagnostic pop")\

#endif // NRS_CC_MACROS_H_
