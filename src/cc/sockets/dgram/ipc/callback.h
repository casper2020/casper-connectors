/**
 * @file client.h
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
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NRS_CC_SOCKETS_DGRAM_IPC_CALLBACK_H_
#define NRS_CC_SOCKETS_DGRAM_IPC_CALLBACK_H_
#pragma once

#include <chrono>
#include <functional>

#include <event2/event.h> // libevent2

namespace cc
{
    
    namespace sockets
    {
        
        namespace dgram
        {
            
            namespace ipc
            {
                
                class Callback final
                {
                    
                public: // Data
                    
                    const void*                           owner_;
                    const int64_t                         timeout_ms_;
                    const bool                            recurrent_;
                    
                private: // Data
                    
                    std::chrono::steady_clock::time_point start_time_point_;
                    struct event*                         event_;
                    timeval                               timeval_;
                    
                private:
                    
                    std::function<void(void* a_payload)>  payload_function_;
                    std::function<void()>                 no_payload_function_;
                    void*                                 payload_;
                    
                public: // Constructor(s) / Destructor
                    
                    /**
                     * @brief Default constructor.
                     *
                     * @param a_owner
                     * @param a_function
                     * @param a_timeout_ms
                     * @param a_recurrent
                     */
                    Callback (const void* a_owner, std::function<void()> a_function, const int64_t a_timeout_ms, const bool a_recurrent = false)
                        : owner_(a_owner), timeout_ms_(a_timeout_ms), recurrent_(a_recurrent)
                    {
                        start_time_point_    = std::chrono::steady_clock::now();
                        event_               = nullptr;
                        timeval_.tv_sec      = 0;
                        timeval_.tv_usec     = 0;
                        no_payload_function_ = std::move(a_function);
                        payload_             = nullptr;
                    }
                    
                    /**
                     * @brief Constructor for callback w/payload.
                     *
                     * @param a_owner
                     * @param a_function
                     * @param a_payload
                     * @param a_timeout_ms
                     * @param a_recurrent
                     */
                    Callback (const void* a_owner, std::function<void(void* a_payload)> a_function, void* a_payload, int64_t a_timeout_ms, const bool a_recurrent = false)
                        : owner_(a_owner), timeout_ms_(a_timeout_ms), recurrent_(a_recurrent)
                    {
                        start_time_point_ = std::chrono::steady_clock::now();
                        event_            = nullptr;
                        timeval_.tv_sec      = 0;
                        timeval_.tv_usec     = 0;
                        payload_function_ = std::move(a_function);
                        payload_          = a_payload;
                    }
                    
                    /**
                     * @brief Destructor.
                     */
                    virtual ~Callback ()
                    {
                        payload_function_    = nullptr;
                        no_payload_function_ = nullptr;
                        if ( nullptr != event_ ) {
                            evtimer_del(event_);
                            event_free(event_);
                        }
                    }
                    
                public: // Method(s) / Function(s)
                    
                    /**
                     * @brief Set a timer.
                     *
                     * @param a_event_base
                     * @param a_function
                     */
                    inline void SetTimer (struct event_base* a_event_base, event_callback_fn a_function)
                    {
                        if ( nullptr != event_ ) {
                            throw ::cc::Exception("Unable to schedule a callback event - event already registered!");
                        }
                        
                        event_ = evtimer_new(a_event_base, a_function, this);
                        if ( nullptr == event_ ) {
                            throw ::cc::Exception("Unable to schedule a callback event - nullptr!");
                        }
                        
                        
                        timeval_.tv_sec  = ( static_cast<int>(timeout_ms_) / 1000 );
                        timeval_.tv_usec = ( ( static_cast<int>(timeout_ms_) - ( static_cast<int>(timeval_.tv_sec)  * 1000 ) ) ) * 1000;
                        
                        const int wd_rv = evtimer_add(event_, &timeval_);
                        if ( wd_rv < 0 ) {
                            throw ::cc::Exception("Unable to schedule a callback event - add error code %d !", wd_rv);
                        }
                    }

                    /**
                     * @brief Call previously registered function.
                     */
                    inline void Call ()
                    {
                        if ( nullptr != no_payload_function_ ) {
                            no_payload_function_();
                            if ( false == recurrent_ ) {
                                no_payload_function_ = nullptr;
                            }
                        } else if ( nullptr != payload_function_ ) {
                            payload_function_(payload_);
                            if ( false == recurrent_ ) {
                                payload_function_ = nullptr;
                            }
                        }
                        if ( true == recurrent_ ) {
                            const int del_rv = evtimer_del(event_);
                            if ( del_rv < 0 ) {
                                throw ::cc::Exception("Unable to schedule a callback event - delete error %d !", del_rv);
                            }
                            const int add_rv = evtimer_add(event_, &timeval_);
                            if ( add_rv < 0 ) {
                                throw ::cc::Exception("Unable to schedule a callback event - add error code %d !", add_rv);
                            }
                        }
                    }
                    
                }; // end of class 'Callback'
                
            } // end of namespace 'ipc'
            
        } // end of namespace 'dgram'
        
    } // end of namespace 'sockets'
    
} // end of namespace 'cc'

#endif // NRS_CC_SOCKETS_DGRAM_IPC_CALLBACK_H_

