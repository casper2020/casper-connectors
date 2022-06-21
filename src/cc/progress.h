/**
 * @file progress.h
 *
 * Copyright (c) 2011-2019 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_PROGRESS_H_
#define NRS_CC_PROGRESS_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

#include "cc/bitwise_enum.h"

#include <chrono>

namespace cc
{

    //
    // Statistics Class
    //
    class Statistics : public ::cc::NonCopyable, public ::cc::NonMovable
    {
        
    public: // Constructor(s) / Destructor
        
        /**
         * @brief Destructor.
        */
        virtual ~Statistics()
        {
            /* empty */
        }
        
    public: // Pure Virtual Method(s) / Functions
        
        /**
         * @brief This function will be called when it's time to reset stats.
         */
        virtual void Reset() = 0;
        
    }; // end of class 'Statistics'

    //
    // Class BaseProgress
    //
    class BaseProgress : public ::cc::NonCopyable, public ::cc::NonMovable
    {
        
    public: // Enum(s)
        
        enum class ResetFlags : uint8_t {
            Statistics    = 0x01,
            Notifications = 0x02,
            Progress      = 0x04,
            All           = 0x07
        };
        
    public: // Constructor(s) / Destructor
        
        /**
         * @brief Destructor.
         */
        virtual ~BaseProgress ()
        {
            /* empty */
        }
        
    public:
        
        /**
         * @brief Reset current progress data.
         *
         * @param a_flags Reset flags bitmask, see \link Reset \link
         */
        virtual void Reset (const ResetFlags a_flags = ResetFlags::All) = 0;
        
    };

    DEFINE_ENUM_WITH_BITWISE_OPERATORS(BaseProgress::ResetFlags)

    //
    // Progress Template
    //
    template <typename S, class Stats, class = std::enable_if<std::is_base_of<Stats, Statistics>::value>> class Progress : public BaseProgress
    {

    public: // Data Type(s)
        
        typedef struct {
            S     stage_;      //!< One of \link S \link
            float percentage_; //!< 0..100
            Stats stats_;      //!< Statistics data.
        } Value;
        
        typedef std::function<void(const Value&)> Callback;
        
    private: // Const Data
        
        const S default_stage_;

    private: // Data

        Value     value_;    //!< Current value(s).
        long long timeout_;  //!< Report timeout in seconds.
        Callback  callback_; //!< Function to call when it's time to deliver a progress report.

    protected: // Data
        
        Stats* stats_;
        
    private: // Data - Notification Control
        
        std::chrono::steady_clock::time_point last_;
                        
    public: // Constructor(s) / Destructor
        
        /**
         * @brief Default constructor.
         */
        Progress () = delete; // not allowed
        
        /**
         * @brief Constructor.
         *
         * @param a_stage   Current stage.
         * @param a_default Default stage, used on reset.
         */
        Progress (const S a_stage, const S a_default)
            : default_stage_(a_default)
        {
            value_.stage_      = a_stage;
            value_.percentage_ = 0.0f;
            timeout_           = 3; // 3 sec
            callback_          = nullptr;
            stats_             = new Stats();
            last_              = std::chrono::steady_clock::time_point::max();
        }
        
        /**
         * @brief Destructor.
         */
        virtual ~Progress ()
        {
            delete stats_;
        }

    public: // Inline Method(s) / Functions
        
        /**
         * @brief Reset current context, except callback and timeout.
         *
         * @param a_flags Reset flags, see \link Reset \link
         */
        virtual void Reset (const ResetFlags a_flags = ResetFlags::All)
        {
            if ( ResetFlags::Progress == ( a_flags & ResetFlags::Progress ) ) {
                value_.percentage_ = 0.0;
                value_.stage_      = default_stage_;
            }
            if ( ResetFlags::Notifications == ( a_flags & ResetFlags::Notifications ) ) {
                last_ = std::chrono::steady_clock::time_point::max();
            }
            if ( ResetFlags::Statistics == ( a_flags & ResetFlags::Statistics ) ) {
                stats_->Reset();
            }
        }

        /**
         * @brief Set current stage.
         *
         * @param a_stage \link S \link.
         * @param a_percentage 0..100
         * @param a_notify When true, elapse between notifications calls will be ignored and notification callback will be called.
         */
        inline void Set (S a_stage, float a_percentage, bool a_notify = false)
        {
            value_.stage_      = a_stage;
            value_.percentage_ = a_percentage;
            Notify(/* a_force */ a_notify);
        }

        /**
         * @brief Set current percentage.
         *
         * @param a_percentage 0..100
         * @param a_notify When true, elapse between notifications calls will be ignored and notification callback will be called.
         */
        inline void Set (float a_percentage, bool a_notify = false)
        {
            value_.percentage_ = a_percentage;
            Notify(/* a_force */ a_notify);
        }
        
        /**
         * @brief Set callback and timeout.
         *
         * @param a_callback Function to call when a request is made to notify current progress value.
         * @param a_timeout  Number of seconds that must elapse between notifications calls.
         */
        inline void Set (Callback& a_callback, size_t a_timeout = 3)
        {
            callback_ = a_callback;
            timeout_  = static_cast<long long>(a_timeout);
        }
        
        /**
         * @brief Call when it'a time to notify.
         *
         * @param a_force When true it will force notification, else if time elapsed its lower than timeout this call will be ignored.
         */
        inline void Notify (bool a_force = false)
        {
            const auto now     = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_).count();
            if ( std::chrono::steady_clock::time_point::max() == last_ || elapsed >= timeout_ || true == a_force ) {
                last_ = now;
                if ( nullptr != callback_ ) {
                    callback_(value_);
                }
            }
        }
        
        /**
         * @return Current value, see \link Value \link.
         */
        inline const Value& value () const
        {
            return value_;
        }
        
        /**
         * @return Read only access to current stats value, see \link Stats \link.
         */
        inline const Stats& stats () const
        {
            return *stats_;
        }
        
        /**
         * @return Current stats value, see \link Stats \link.
         */
        inline Stats& stats ()
        {
            return *stats_;
        }

    }; // end of class 'Progress'

} // end of namespace 'cc'

#endif // NRS_CC_PROGRESS_H_
