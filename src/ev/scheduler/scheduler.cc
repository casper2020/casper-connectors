/**
 * @file scheduler.cc
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

#include "ev/scheduler/scheduler.h"

#include "ev/exception.h"

#include <sys/types.h>
#include <unistd.h> // getpid

#include <sstream>   // std::stringstream
#include <algorithm> // std::find_if

ev::hub::Hub* ev::scheduler::Scheduler::hub_        = nullptr;
ev::Bridge*   ev::scheduler::Scheduler::bridge_ptr_ = nullptr;

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Start event scheduler.
 *
 * @param a_socket_fn
 * @param a_bridge
 * @param a_initialized_callback
 * @param a_device_factory
 * @param a_device_limits
 */
void ev::scheduler::Scheduler::Scheduler::Start (const std::string& a_name,
                                                 const std::string& a_socket_fn,
                                                 ev::Bridge& a_bridge,
                                                 ev::scheduler::Scheduler::InitializedCallback a_initialized_callback,
                                                 ev::scheduler::Scheduler::DeviceFactoryCallback a_device_factory,
                                                 ev::scheduler::Scheduler::DeviceLimitsCallback a_device_limits)
{

    OSALITE_DEBUG_TRACE("ev_scheduler", "~> Start(...)");
    
    socket_fn_ = a_socket_fn;
    
    pending_callbacks_count_ = 0;
    
    bridge_ptr_ = &a_bridge;
    
    hub_ = new ev::hub::Hub(a_name, a_bridge, socket_fn_, pending_callbacks_count_);
    hub_->Start(
                [this, a_initialized_callback]() {
                    
                    //
                    // REMARKS:
                    //          this callback will be called once, as soon as hub thread is ready to accept requests
                    //
                    if ( false == socket_.Create(socket_fn_) ) {
                        throw ev::Exception("Unable to start scheduler: can't open a socket, using '%s' file!",
                                            socket_fn_.c_str());
                    }
                    
                    // ... 'this' side socket must be binded now ...
                    if ( true == socket_.Bind() ) {
                        // ... we're good to go ....
                        a_initialized_callback();
                    } else {
                        // ... unable to bind socket ...
                        throw ev::Exception("Unable to bind client socket: %s",socket_.GetLastConfigErrorString().c_str());
                    }
                    
                },
                [this] (const int64_t a_invoke_id, const ev::Object::Target /* a_target */, const uint8_t a_tag, ev::Result* a_result) -> bool {

                    //
                    // NEXT STEP:
                    //
                    auto ids_to_object_map_it = ids_to_object_map_.find(a_invoke_id);
                    if ( ids_to_object_map_.end() == ids_to_object_map_it ) {
                        // ... since the object no longer exists ...
                        // ... by returning false, a_result will be released  ...
                        return false;
                    }

                    const ev::scheduler::Object::Type type = static_cast<ev::scheduler::Object::Type>(a_tag);
                    if ( ev::scheduler::Object::Type::Task == type || ev::scheduler::Object::Type::Subscription == type ) {
                        
                        //
                        //  REMARKS:
                        //           this callback will be called each time an object request returned
                        //           by returning true \link a_result \link ownership MUST be moved to 'this' object
                        
                        ev::scheduler::Object* object = ids_to_object_map_it->second;

                        //
                        // ... check if it was 'detached' ...
                        //
                        const auto detached_it = std::find_if(detached_.begin(), detached_.end(), [object](const ev::scheduler::Object* a_s_object) {
                            return ( a_s_object == object );
                        });
                        if ( detached_.end() != detached_it ) {
                            // ... object is detached ...
                            ReleaseObject(object);
                            // ... a_result not accepted ...
                            return false;
                        }

                        // ... client still waiting for this object?
                        auto objects_to_client_map_it = object_to_client_map_.find(object->UniqueID());
                        if ( object_to_client_map_.end() == objects_to_client_map_it ) {
                            // ... no longer waiting ...
                            ReleaseObject(object);
                            // ... a_result not accepted ...
                            return false;
                        }

                        //
                        // ... a_result:
                        //
                        //  - contains the previous step result ...
                        //  - it ownership will be transfered to the task that requested it ...
                        //
                        ev::Request* next_request = nullptr;
                        if ( true == object->Step(a_result, &next_request) ) {
                            // ... finished ... object can be released now ...
                            ReleaseObject(object);
                            // ...
                            return true;
                        } else if ( nullptr != next_request ) {
                            (void)std::atomic_fetch_add(&pending_callbacks_count_, 1);
                            // <invoke_id>:<mode>:<target>:<tag>:<obj_addr>
                            if ( false == socket_.Send(ev::hub::Hub::k_msg_with_payload_format_,
                                                       object->UniqueID(), next_request->mode_, next_request->target_, object->type_, next_request) ) {
                                throw ev::Exception("1) Unable to send a message through socket: %s!",
                                                    socket_.GetLastSendErrorString().c_str()
                                );
                            }
                        }
                        // ... a_result accepted ....
                        return true;
                    }
                    
                    // ... a_result rejected ...
                    return false;
                },
                [this] (const int64_t a_invoke_id, const ev::Object::Target /* a_target */, const uint8_t a_tag, std::vector<ev::Result*>& a_results) {
                    //
                    // PUBLISH STEP
                    //
                    auto ids_to_object_map_it = ids_to_object_map_.find(a_invoke_id);
                    if ( ids_to_object_map_.end() == ids_to_object_map_it ) {
                        // ... since the object no longer exists ...
                        return;
                    }
                    
                    const ev::scheduler::Object::Type type = static_cast<ev::scheduler::Object::Type>(a_tag);
                    if ( ev::scheduler::Object::Type::Subscription != type ) {
                        return;
                    }
                    
                    ev::scheduler::Subscription* subscription_object = dynamic_cast<ev::scheduler::Subscription*>(ids_to_object_map_it->second);
                    if ( nullptr == subscription_object ) {
                        throw ev::Exception("Logic error: expecting subscription object!");
                    }
                    
                    subscription_object->Publish(a_results);                    
                },
                [this] (const int64_t a_invoke_id, const ev::Object::Target /* a_target */, const uint8_t a_tag) {
                    //
                    // DISCONNECTED STEP
                    //
                    const ev::scheduler::Object::Type type = static_cast<ev::scheduler::Object::Type>(a_tag);
                    if ( ev::scheduler::Object::Type::Task == type || ev::scheduler::Object::Type::Subscription == type ) {
                        auto ids_to_object_map_it = ids_to_object_map_.find(a_invoke_id);
                        if ( ids_to_object_map_.end() == ids_to_object_map_it ) {
                            // ... since the object no longer exists ...
                            return;
                        }
                        
                        ev::scheduler::Object* object = ids_to_object_map_it->second;
                        
                        // ... notify and check if object should be release now ...
                        if ( true == ids_to_object_map_it->second->Disconnected() ) {
                            // ... object can be release now ...
                            ReleaseObject(object);
                        }
                    }
                },
                a_device_factory,
                a_device_limits
    );
    
    OSALITE_DEBUG_TRACE("ev_scheduler", "<~ Start(...)");
}

/**
 * @brief Stop event scheduler.
 *
 * @param a_finalization_callback
 * @param a_sig_no
 */
void ev::scheduler::Scheduler::Stop (ev::scheduler::Scheduler::FinalizationCallback a_finalization_callback,
                                     int a_sig_no)
{
    OSALITE_DEBUG_TRACE("ev_scheduler", "~> Stop(...)");
    
    if ( nullptr != hub_ ) {
        hub_->Stop(a_sig_no);
        delete hub_;
    }

    for ( auto vector : { &zombies_, &detached_ } ) {
        for ( auto object : *vector ) {
            delete object;
        }
        vector->clear();
    }
    
    clients_to_objects_map_.clear();
    object_to_client_map_.clear();
    ids_to_object_map_.clear();

    if ( nullptr != a_finalization_callback ) {
      a_finalization_callback();
    }
    
    OSALITE_DEBUG_TRACE("ev_scheduler", "<~ Stop(...)");
}

/**
 * @brief Add an object to be execured asynchronously.
 *
 * @param a_client
 * @param a_object
 */
void ev::scheduler::Scheduler::Push (ev::scheduler::Client* a_client, ev::scheduler::Object* a_object)
{
    if ( nullptr == hub_ ) {
        throw ev::Exception("Can't add a new object to scheduler - hub is not running!");
    }
    
    const auto it = clients_to_objects_map_.find(a_client->id());
    if ( clients_to_objects_map_.end() == it ) {
        throw ev::Exception("Client %p not registered @ events scheduler!", (void*)a_client);
    }
    
    // ... object already exist?
    const auto o_it = std::find_if(it->second->begin(), it->second->end(), [a_object](const ev::scheduler::Object* a_it_object) {
        return ( a_it_object == a_object );
    });
    if ( it->second->end() != o_it ) {

        (void)std::atomic_fetch_add(&pending_callbacks_count_, 1);
        // <invoke_id>:<mode>:<target>:<tag>
        if ( false == socket_.Send(ev::hub::Hub::k_msg_no_payload_format_,
                                   a_object->UniqueID(), ev::Request::Mode::NotSet, ev::Object::Target::NotSet, a_object->type_) ) {
            throw ev::Exception("2) Unable to send a message through socket: %s!",
                                socket_.GetLastSendErrorString().c_str()
            );
        }

    } else {
        // ... new object ....
        it->second->push_back(a_object);

        const uint64_t object_unique_id = a_object->UniqueID();

        object_to_client_map_[object_unique_id] = a_client;
        ids_to_object_map_[object_unique_id]    = a_object;
        
        (void)std::atomic_fetch_add(&pending_callbacks_count_, 1);
        // <invoke_id>:<mode>:<target>:<tag>
        if ( false == socket_.Send(ev::hub::Hub::k_msg_no_payload_format_,
                                   object_unique_id, ev::Request::Mode::NotSet, ev::Object::Target::NotSet, a_object->type_) ) {
            throw ev::Exception("3) Unable to send a message through socket: %s!",
                                socket_.GetLastSendErrorString().c_str()
            );
        }
    }
}

/**
 * @brief Setup scheduler for a new client.
 *
 * @param a_client
 */
void ev::scheduler::Scheduler::Register (ev::scheduler::Client* a_client)
{
    auto it = clients_to_objects_map_.find(a_client->id());
    if ( clients_to_objects_map_.end() != it ) {
        return;
    }
    clients_to_objects_map_[a_client->id()] = new ev::scheduler::Scheduler::ObjectsVector();
    // ... get rid of 'zombie' objects ...
    KillZombies();
}

/**
 * @brief Detach and destroy all objects for a client.
 *
 * @param a_client
 */
void ev::scheduler::Scheduler::Unregister (ev::scheduler::Client* a_client)
{
    const auto it = clients_to_objects_map_.find(a_client->id());
    if ( clients_to_objects_map_.end() == it ) {
        return;
    }
    for ( auto object : *it->second ) {
        detached_.push_back(object);
    }
    delete it->second;
    clients_to_objects_map_.erase(it);
    // [B] AG: NOV 26, 2018
    std::vector<uint64_t> to_erase;
    for ( auto it = object_to_client_map_.begin() ; object_to_client_map_.end() != it ; ++it ) {
        if ( it->second == a_client ) {
            to_erase.push_back(it->first);
        }
    }
    for ( auto obj_id : to_erase ) {
        object_to_client_map_.erase(object_to_client_map_.find(obj_id));
    }
    // [E] AG: NOV 26, 2018
    // ... unregister timeout callbacks ...
    const auto timeout_callback_it = pending_timeouts_.find(a_client->id());
    if ( pending_timeouts_.end() != timeout_callback_it ) {
        pending_timeouts_.erase(timeout_callback_it);
    }
    // ... get rid of 'zombie' objects ...
    KillZombies();
}

/**
 * Register a timeout for a specific client.
 *
 * @param a_client
 * @param a_ms
 * @param a_callback
 */
void ev::scheduler::Scheduler::SetClientTimeout (ev::scheduler::Client* a_client,
                                                 uint64_t a_ms, ev::scheduler::Scheduler::TimeoutCallback a_callback)
{
    const std::string id = a_client->id();
    // ... register as pending callback ...
    pending_timeouts_.insert(id);
    // ... schedule callback ...
    bridge_ptr_->CallOnMainThread(
                                  /* a_callback */
                                  [this, id, a_callback] () {
                                      // ... if ...
                                      const auto t_it = pending_timeouts_.find(id);
                                      if ( pending_timeouts_.end() == t_it ) {
                                          // ... timeout was cancelled, function can't be called ...
                                          return;
                                      }
                                      // ... timeout was not cancelled, call function now ...
                                      a_callback();
                                  },
                                  /* a_timeout_ms */
                                  a_ms
    );
}

/**
 * Execute a callback on main thread.
 *
 * @param a_client
 * @param a_callback
 * @param a_timeout
 */
void ev::scheduler::Scheduler::CallOnMainThread (Client* a_client, std::function<void()> a_callback, int64_t a_timeout_ms)
{
    bridge_ptr_->CallOnMainThread(
                                  /* a_callback */
                                  [this, a_client, a_callback] () {
                                      const auto it = clients_to_objects_map_.find(a_client->id());
                                      if ( clients_to_objects_map_.end() == it ) {
                                          return;
                                      }
                                      a_callback();
                                  },
                                  /* a_timeout_ms */
                                  a_timeout_ms
    );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Delete all taks that have no parent.
 */
void ev::scheduler::Scheduler::KillZombies ()
{
    for ( auto zombie : zombies_ ) {
        delete zombie;
    }
    zombies_.clear();
}

/**
 * @brief This method will be called when an object was released is completed.
 *
 * @param a_object
 */
void ev::scheduler::Scheduler::ReleaseObject (ev::scheduler::Object* a_object)
{
    // ... first check if this object is a 'zombie'
    for ( auto it = zombies_.begin(); zombies_.end() != it ; ++it ) {
        if ( a_object == (*it) ) {
            // ... it's a zombie ...
            delete a_object;
            zombies_.erase(it);
            break;
        }
    }
    // ... not a zombie, check if it was detached ...
    for ( auto it = detached_.begin(); detached_.end() != it ; ++it ) {
        if ( a_object == (*it) ) {
            // ... detached, promote it to a zombie ...
            detached_.erase(it);
            break;
        }
    }
    
    bool is_attached = false;
    
    // ... check if it's attached to a client ...
    auto objects_to_client_map_it = object_to_client_map_.find(a_object->UniqueID());
    if ( object_to_client_map_.end() != objects_to_client_map_it ) {
        auto clients_to_objects_map_it = clients_to_objects_map_.find(objects_to_client_map_it->second->id());
        if ( clients_to_objects_map_.end() != clients_to_objects_map_it ) {
            for ( auto clients_objects_it = clients_to_objects_map_it->second->begin(); clients_to_objects_map_it->second->end() != clients_objects_it ; ++clients_objects_it ) {
                if ( (*clients_objects_it) == a_object ) {
                    clients_to_objects_map_it->second->erase(clients_objects_it);
                    is_attached = true;
                    break;
                }
            }
        }
        object_to_client_map_.erase(objects_to_client_map_it);
    }
    
    auto ids_to_object_map_it = ids_to_object_map_.find(a_object->UniqueID());
    if ( ids_to_object_map_.end() != ids_to_object_map_it ) {
        ids_to_object_map_.erase(ids_to_object_map_it);
    }
    
    // ... is a zombie or can be deleted now?
    if ( false == is_attached ) {
        zombies_.push_back(a_object);
    } else {
        delete a_object;
    }
}
