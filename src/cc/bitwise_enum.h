/**
 * @file version.h
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
#ifndef NRS_CC_BITWISE_ENUM_H_
#define NRS_CC_BITWISE_ENUM_H_

#define NRS_CC_BITWISE_ENUM_FRIEND
// friend

#define DEFINE_ENUM_WITH_BITWISE_OPERATORS(x)  \
\
    template<typename Enum> struct EnumWithBitWiseOperators \
    { \
        static const bool enable_ = false; \
    }; \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator | (Enum lhs, Enum rhs) {					\
        using underlying = typename std::underlying_type<Enum>::type; \
        return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); \
    } \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator & (Enum lhs, Enum rhs) { \
        using underlying = typename std::underlying_type<Enum>::type; \
        return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); \
    } \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator ^ (Enum lhs, Enum rhs) { \
    using underlying = typename std::underlying_type<Enum>::type; \
        return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)); \
    } \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator ~ (Enum rhs) { \
        return static_cast<Enum>(~static_cast<typename std::underlying_type<Enum>::type>(rhs)); \
    } \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum&>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator |= (Enum& lhs, Enum rhs) { \
        using underlying = typename std::underlying_type<Enum>::type; \
        lhs = static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); \
        return lhs; \
    } \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum&>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator &= (Enum& lhs, Enum rhs) { \
        using underlying = typename std::underlying_type<Enum>::type; \
        lhs = static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); \
        return lhs; \
    } \
\
    template<typename Enum> \
    typename std::enable_if<EnumWithBitWiseOperators<Enum>::enabled_, Enum&>::type \
    NRS_CC_BITWISE_ENUM_FRIEND operator ^= (Enum& lhs, Enum rhs) { \
        using underlying = typename std::underlying_type<Enum>::type; \
        lhs = static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)); \
        return lhs; \
    } \
\
    template<> struct EnumWithBitWiseOperators<x> \
    { \
        static const bool enabled_ = true; \
    };

#endif // NRS_CC_BITWISE_ENUM_H_
