/**
 * @file scheduler.h
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
#ifndef NRS_EV_SCHEDULER_H_
#define NRS_EV_SCHEDULER_H_

#include "osal/osalite.h"

#include "osal/datagram_socket.h"

#include "ev/hub/hub.h"
#include "ev/scheduler/client.h"
#include "ev/scheduler/task.h"
#include "ev/scheduler/subscription.h"

#include <queue>  // std::queue
#include <vector> // std::vector
#include <map>    // std::map
#include <functional> // std::function

namespace ev
{
 
    namespace scheduler
    {
    
        // ---- //
        class Scheduler;
        class SchedulerOneShot final : public ::osal::Initializer<Scheduler>
        {
            
        public: // Constructor(s) / Destructor
            
            SchedulerOneShot (Scheduler& a_instance)
                : ::osal::Initializer<Scheduler>(a_instance)
            {
                /* empty */
            }
            virtual ~SchedulerOneShot ()
            {
                /* empty */
            }
            
        }; // end of class 'UniqueIDGenerator'

        class Scheduler final : public osal::Singleton<Scheduler, SchedulerOneShot>
        {
            
            typedef hub::Hub::InitializedCallback  InitializedCallback;
            typedef hub::DeviceFactoryStepCallback DeviceFactoryCallback;
            typedef hub::DeviceLimitsStepCallback  DeviceLimitsCallback;
            typedef InitializedCallback            FinalizationCallback;
            typedef std::function<void()>          TimeoutCallback;
            
        protected: // Data Type(s)
            
            typedef std::vector<scheduler::Object*>       ObjectsVector;
            typedef std::map<std::string, ObjectsVector*> ClientsToObjectMap;
            typedef std::map<int64_t, Client*>            ObjectsToClientMap;
            typedef std::map<int64_t, scheduler::Object*> IdsToObjectMap;
            
        protected: // Static Data
            
            static ev::hub::Hub*               hub_;
            static ev::Bridge*                 bridge_ptr_;
            
        protected: // Data
            
            ClientsToObjectMap                 clients_to_objects_map_;
            ObjectsToClientMap                 object_to_client_map_;
            IdsToObjectMap                     ids_to_object_map_;
            ObjectsVector                      detached_;
            ObjectsVector                      zombies_;
            
            osal::DatagramClientSocket         socket_;
            std::string                        socket_fn_;
            
            std::atomic<int>                   pending_callbacks_count_;
            
        private: // Data
            
            std::set<std::string>              pending_timeouts_;
            
        public: // Method(s) / Function(s)
            
            void Start      (const std::string& a_name,
                             const std::string& a_socket_fn,
                             ev::Bridge& a_bridge, InitializedCallback a_initialized_callback, DeviceFactoryCallback a_device_factory, DeviceLimitsCallback a_device_limits);
            void Stop       (FinalizationCallback a_finalization_callback,
                             int a_sig_no);
            void Push       (Client* a_client, scheduler::Object* a_task);
            
            bool IsRegistered (Client* a_client);
            void Register     (Client* a_client);
            void Unregister   (Client* a_client);
            
            void SetClientTimeout (Client* a_client, uint64_t a_ms, TimeoutCallback a_callback);
            
            void CallOnMainThread (Client* a_client, std::function<void()> a_callback,int64_t a_timeout_ms = 0);
            
        public:
            
            bool IsInitialized () const;
            
        protected: // Method(s) / Function(s)
            
            bool IsDetached     (const scheduler::Object* a_object) const;
            bool IsZombie       (const scheduler::Object* a_object) const;
            
            void KillZombies    ();
            void ReleaseObject  (scheduler::Object* a_object);
            
        }; // end of class 'Scheduler'
        
        /**
         * @return True if this object was properly initialized, false otherwise.
         */
        inline bool Scheduler::IsInitialized () const
        {
            return nullptr != hub_;
        }
        
        /**
         * @brief Check if an object was tracked as 'detached'.
         *
         * @param a_object Object to check.
         *
         * @return True of object was tracked as a 'detached', false otherwise.
         */
        inline bool Scheduler::IsDetached (const scheduler::Object* a_object) const
        {
            const auto it = std::find_if(detached_.begin(), detached_.end(), [a_object](const ev::scheduler::Object* a_s_object) {
                return ( a_s_object == a_object );
            });
            return detached_.end() != it;
        }
        
        /**
         * @brief Check if an object was tracked as 'zombie'.
         *
         * @param a_object Object to check.
         *
         * @return True of object was tracked as a 'zombie', false otherwise.
         */
        inline bool Scheduler::IsZombie (const scheduler::Object* a_object) const
        {
            const auto it = std::find_if(zombies_.begin(), zombies_.end(), [a_object](const ev::scheduler::Object* a_s_object) {
                return ( a_s_object == a_object );
            });
            return zombies_.end() != it;
        }
        
    } // end of namespace 'scheduler'
    
} // end of namespace ev

#endif // NRS_EV_MANAGER_H_
