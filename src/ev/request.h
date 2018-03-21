/**
 * @file request.h
 *
 * Copyright (c) 2011-2018 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_EV_REQUEST_H_
#define NRS_EV_REQUEST_H_

#include "ev/object.h"
#include "ev/result.h"
#include "ev/loggable.h"

#include <vector>     // std::vector
#include <chrono>     // std::chrono::steady_clock::time_point
#include <functional> // std::function

namespace ev
{
    
    class Request : public Object
    {
        
        friend class Task;

    public: // Data Type(s)
        
        enum class Mode : uint8_t
        {
            NotSet,
            KeepAlive,
            OneShot
        };
        
        enum class Control : uint8_t
        {
            NotSet,
            Invalidate
        };
        
    public:
        
        const Loggable::Data loggable_data_;
        const Mode           mode_;
        const Control        control_;

    protected: // Data
        
        int64_t                               invoke_id_;
        uint8_t                               tag_;
        Result*                               result_;

    private: // Timeout Related
        
        std::chrono::steady_clock::time_point start_time_point_;
        long                                  timeout_in_ms_;
        std::function<void()>                 timeout_callback_;
        
    public: // Constructor(s) / Destructor
        
        Request (const Loggable::Data& a_loggable_data,
                 const Object::Target a_target, const Request::Mode a_mode,
                 const Request::Control a_control = ev::Request::Control::NotSet);
        virtual ~Request ();
        
    public: // Method(s) / Function(s)

        void    Set          (const int64_t a_id, const uint8_t a_tag);
        int64_t GetInvokeID  () const;
        uint8_t GetTag       () const;

        void    AttachResult (Result* a_result);
        Result* DetachResult ();

        void SetTimeout      (const long a_ms, std::function<void()> a_callback);
        bool CheckForTimeout (const std::chrono::steady_clock::time_point& a_time_point) const;

    };
    
    /**
     * @brief Set an invoke id and a tag.
     *
     * @param a_invoke_id
     * @param a_tag
     */
    inline void Request::Set (const int64_t a_invoke_id, const uint8_t a_tag)
    {
        invoke_id_ = a_invoke_id;
        tag_       = a_tag;
    }

    /**
     * @return The previously stored invoke id.
     */
    inline int64_t Request::GetInvokeID () const
    {
        return invoke_id_;
    }
    
    /**
     * @return The previously set tag.
     */
    inline uint8_t Request::GetTag () const
    {
        return tag_;
    }
    
    /**
     * @brief Attach a result object, it's memory ownership is now this object responsability.
     *
     * @param a_result
     */
    inline void Request::AttachResult (Result* a_result)
    {
        if ( nullptr != result_ ) {
            delete result_;
        }
        result_ = a_result;
    }    
    
    /**
     * @brief Detach a result object.
     *
     * @return The result object, it's memory ownership is now responsability if this method caller.
     *
     * @remarks Previous result, if any, will be deleted!
     */
    inline Result* Request::DetachResult ()
    {
        Result* rv = result_;
        result_ = nullptr;
        return rv;
    }
    
    /**
     * @brief Set timeout in miliseconds and a callback.
     *
     * @param a_ms
     * @param a_callback
     */
    inline void Request::SetTimeout (const long a_ms, std::function<void()> a_callback)
    {
        start_time_point_ = std::chrono::steady_clock::now();
        timeout_in_ms_    = a_ms;
        timeout_callback_ = a_callback;
    }
    
    /**
     * @brief Check for a timeout.
     *
     * @param a_time_point
     *
     * @return True if timeout occurred, false otherwise.
     */
    inline bool Request::CheckForTimeout (const std::chrono::steady_clock::time_point& a_time_point) const
    {
        if ( 0 == timeout_in_ms_ ) {
            return false;
        }
        const bool timeout = ( std::chrono::duration_cast<std::chrono::milliseconds>(a_time_point - start_time_point_).count() > timeout_in_ms_ );
        if ( true == timeout  ) {
            if ( nullptr != timeout_callback_ ) {
                timeout_callback_();
            }
        }
        return timeout;
    }
    
} // end of namespace 'ev'

#endif // NRS_EV_REQUEST_H_
