/**
 * @file device.h
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
#ifndef NRS_EV_DEVICE_H_
#define NRS_EV_DEVICE_H_

#include "ev/object.h"
#include "ev/request.h"
#include "ev/result.h"
#include "ev/request.h"
#include "ev/exception.h"
#include "ev/error.h"
#include "ev/loggable.h"

#include <functional> // std::function
#include <string>     // std::string
#include <vector>     // std::vector
#include <limits>     // std::numeric_limits

#include <event2/event.h> // libevent2

namespace ev
{
    
    /**
     * @brief A class that defines a 'device' that connects to a 'hub'.
     */
    class Device
    {
        
    public: // Data Type(s)

        enum class Status : uint8_t
        {
            Nop,
            Async,
            Error,
            OutOfMemory
        };
        
        enum class ConnectionStatus : uint8_t
        {
            Error,
            Connected,
            Disconnected
        };

        enum class ExecutionStatus : uint8_t
        {
            Ok,
            Error
        };
        
        class Listener
        {
            
        public: // Constructor(s) / Destructor
            
            /**
             * @brief Default destructor.
             */
            virtual ~Listener ()
            {
                /* empty */
            }
            
        public: // Pure Virtual Method(s) / Function(s) - Declaration
            
            /**
             * @brief This method will be called when a device connection status has changed.
             *
             * @param a_status The new device connection status, one of \link ev::Device::ConnectionStatus \link.
             * @param a_device The device.
             */
            virtual void OnConnectionStatusChanged (const ConnectionStatus& a_status, ev::Device* a_device) = 0;
            
        };
        
        class Handler
        {
            
        public: // Constructor(s) / Destructor
            
            /**
             * @brief Default destructor.
             */
            virtual ~Handler ()
            {
                /* empty */
            }
            
            
        public: // Pure Virtual Method(s) / Function(s) - Declaration
            
            /**
             * @brief This method will be called when a device received a result object and no one collected it.
             *
             * @param a_device  Device that received the unhanled data object.
             * @param a_request Request performed by the device.
             * @param a_result  Request result object that contains the unhandled data object.
             *
             * @return True when the object ownership is accepted, otherwise it will be relased the this function caller.
             */
            virtual bool OnUnhandledDataObjectReceived (const ev::Device* a_device, const ev::Request* a_request, ev::Result* a_result) = 0;

        };
        

        typedef std::function<void(const ConnectionStatus& a_status, ev::Device* a_device)> ConnectedCallback;
        typedef std::function<void(const ConnectionStatus& a_status, ev::Device* a_device)> DisconnectedCallback;
        typedef std::function<void(const ExecutionStatus& a_status,  ev::Result* a_result)> ExecuteCallback;
        typedef std::function<void(const ev::Exception& a_ev_exception)>                    ExceptionCallback;
        
    protected: // Const Data
        
        const Loggable::Data   loggable_data_;

    protected: // Data
        
        int64_t                last_error_code_;
        std::string            last_error_msg_;
        ConnectedCallback      connected_callback_;
        DisconnectedCallback   disconnected_callback_;
        ExecuteCallback        execute_callback_;
        ExceptionCallback      exception_callback_;

        Listener*              listener_ptr_;
        Handler*               handler_ptr_;
        struct event_base*     event_base_ptr_;
        ConnectionStatus       connection_status_;
        ssize_t                reuse_count_;
        ssize_t                max_reuse_count_;
        bool                   tracked_;
        bool                   invalidate_reuse_;

    public: // Constructor(s) / Destructor
        
        Device (const Loggable::Data& a_loggable_data);
        virtual ~Device ();
        
    public: // Virtual Method(s) / Function(s)
        
        virtual void Setup (struct event_base* a_event, ExceptionCallback a_exception_callback);

    public: // Pure Virtual Method(s) / Function(s)
        
        virtual Status Connect         (ConnectedCallback a_callback) = 0;
        virtual Status Disconnect      (DisconnectedCallback a_callback) = 0;
        virtual Status Execute         (ExecuteCallback a_callback, const ev::Request* a_request) = 0;
        virtual Error* DetachLastError () = 0;
        
    public:
        
        void    SetListener        (Listener* a_listener);
        void    SetHandler         (Handler* a_handler);
        void    IncreaseReuseCount ();
        ssize_t MaxReuse           () const;
        ssize_t ReuseCount         () const;
        void    InvalidateReuse    ();
        bool    Reusable           () const;
        bool    Tracked            () const;
        void    SetUntracked       ();
        
    }; // end of class 'Device'
    
    /**
     * @brief Add a listener.
     *
     * @param a_listener See \link ev::Device::Listener \link.
     */
    inline void Device::SetListener (ev::Device::Listener *a_listener)
    {
        listener_ptr_ = a_listener;
    }
    
    /**
     * @brief Set a handler for unhandled events.
     *
     * @param a_handler See \link ev::Device::Handler \link.
     */
    inline void Device::SetHandler (ev::Device::Handler* a_handler)
    {
        handler_ptr_ = a_handler;
    }
    
    /**
     * @brief Increase the reuse counter.
     */
    inline void Device::IncreaseReuseCount ()
    {
        if ( reuse_count_ < std::numeric_limits<ssize_t>::max() ) {
            reuse_count_++;
        }
    }
    
    /**
     * @return The maximum number of times this device can be reused ( -1 if no limit is set ).
     */
    inline ssize_t Device::MaxReuse () const
    {
        return max_reuse_count_;
    }
    
    /**
     * @return The number of time this device connection was used.
     */
    inline ssize_t Device::ReuseCount () const
    {
        return reuse_count_;
    }
    
    /**
     * @brief Increase reuse counter to it's maximum value so it can be considered NOT reusable.
     */
    inline void Device::InvalidateReuse ()
    {
        invalidate_reuse_ = true;
    }

    /**
     * @brief Check if this device is reusable.
     *
     * @return True if so, false otherwise.
     */
    inline bool Device::Reusable () const
    {
        return ( false == invalidate_reuse_ &&  ( -1 == max_reuse_count_ || ( reuse_count_ < max_reuse_count_ ) ) );
    }
    
    /**
     * @return True if the device is being tracked, false otherwise.
     */
    inline bool Device::Tracked () const
    {
        return tracked_;
    }
    
    /**
     * @brief Mark as not being tracked.
     */
    inline void Device::SetUntracked ()
    {
        tracked_ = false;
    }
    
} // end of namespace 'ev'

#endif // NRS_EV_DEVICE_H_
