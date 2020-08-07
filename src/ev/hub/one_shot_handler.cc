/**
 * @file one_shot_handler.cc
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

#include "ev/hub/one_shot_handler.h"

#include <sstream>
#include <algorithm> // std::find_if

#include "osal/osalite.h"

/**
 * @brief Default constructor.
 *
 * @param a_stepper_callbacks
 * @param a_thread_id
 */
ev::hub::OneShotHandler::OneShotHandler (ev::hub::StepperCallbacks& a_stepper_callbacks, cc::debug::Threading::ThreadID a_thread_id)
    : ev::hub::Handler(a_stepper_callbacks, a_thread_id)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    supported_target_ = { ev::Object::Target::Redis, ev::Object::Target::PostgreSQL, ev::Object::Target::CURL };
    for ( auto map : { &cached_devices_, &in_use_devices_ } ) {
        for ( auto target : supported_target_ ) {
            auto it = (*map).find(target);
            if ( (*map).end() == it ) {
                (*map)[target] = new std::vector<ev::Device*>();
            }
            devices_limits_[target] = a_stepper_callbacks.limits_(target);
        }
    }
}

/**
 * @brief Destructor.
 */
ev::hub::OneShotHandler::~OneShotHandler ()
{
    // TODO URGENT CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... get rid of 'zombies' objects ...
    KillZombies();

    // ... release devices ...
    for ( auto map : { &cached_devices_, &in_use_devices_ } ) {
        for ( auto type : supported_target_ ) {
            auto it = (*map).find(type);
            if ( (*map).end() == it ) {
                continue;
            }
            while ( it->second->size() > 0 ) {
                delete (*it->second)[0];
                it->second->erase(it->second->begin());
            }
            delete it->second;
        }
        (*map).clear();
    }

    // ... just ptrs ...
    device_request_map_.clear();
    request_device_map_.clear();
}

#ifdef __APPLE__
#pragma mark - Inherited Virtual Method(s) / Function(s) - from ev::hub::Handler
#endif

/**
 * @brief This method will be called when the hub is idle.
 */
void ev::hub::OneShotHandler::Idle ()
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... push next ...
    Push();
    // ... publish pending ...
    Publish();
}

/**
 * @brief Push a request in to this handler, ( it's memory will be managed by this object ).
 *
 * @param a_request
 */
void ev::hub::OneShotHandler::Push (ev::Request* a_request)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    // ... keep track of it ...
    pending_requests_.push_back(a_request);
    // ... push next ...
    Push();
}

/**
 * @brief Perform a sanity check.
 */
void ev::hub::OneShotHandler::SanityCheck ()
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

    std::map<ev::Device*, size_t>  tmp_devices_map;
    std::map<ev::Request*, size_t> tmp_requests_map;

    //
    // 1st check
    //

    // ... count ...
    for ( auto map : { &cached_devices_, &in_use_devices_ } ) {

        for ( auto type : supported_target_ ) {

            auto it = (*map).find(type);
            if ( (*map).end() == it ) {
                continue;
            }

            for ( auto device : *it->second ) {
                auto tmp_map_it = tmp_devices_map.find(device);
                if ( tmp_devices_map.end() != tmp_map_it ) {
                    tmp_devices_map[device] = tmp_map_it->second + 1;
                } else {
                    tmp_devices_map[device] = 0;
                }
            }

        }

    }
    // ... check ...
    for ( auto it : tmp_devices_map ) {
        // ... if a device has more than one referece ...
        if ( it.second > 1 ) {
            // ... control maps are messed up ...
            throw ev::Exception("Device %p has more than one reference in control maps!",
                                static_cast<void*>(it.first)
            );
        }

    }
    tmp_devices_map.clear();

#ifdef OSALITE_DEBUG
    //
    // 2nd check
    //
    std::stringstream ss;
    std::string       fault_msg;

    // ... a request can only be located in one of the lists ...

    std::deque<ev::Request*> pending_requests;
    for ( auto request : pending_requests_ ) {
        pending_requests.push_back(request);
    }

    const auto deques = { &pending_requests, &completed_requests_, &rejected_requests_ };
    // ... zero out ...
    for ( auto deque : deques ) {
        for ( auto request : *deque ) {
            tmp_requests_map[request] = 0;
        }
    }
    // ... count ...
    for ( auto deque : deques ) {
        for ( auto request : *deque ) {
            tmp_requests_map[request] = tmp_requests_map[request] + 1;
            if ( tmp_requests_map[request] < 1 ) {
                // ... control maps are messed up ...
                ss << "Request " << static_cast<void*>(request) << " has more than one reference in control queues!";
                fault_msg = ss.str();
                // ... nothing else we can do here ...
                break;
            }
        }
        if ( 0 != fault_msg.length() ) {
            break;
        }
    }
    tmp_requests_map.clear();

    // ... report fault ...
    if ( 0 != fault_msg.length() ) {
        throw ev::Exception(fault_msg);
    }
#endif
}

#ifdef __APPLE__
#pragma mark - Inherited Virtual Method(s) / Function(s) from ev::Device::Listener
#endif

/**
 * @brief This method will be called when a device connection status has changed.
 *
 * @param a_status The new device connection status, one of \link ev::Device::ConnectionStatus \link.
 * @param a_device The device.
 */
void ev::hub::OneShotHandler::OnConnectionStatusChanged (const ev::Device::ConnectionStatus& a_status, ev::Device* a_device)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

    std::stringstream ss;

    // ... if device is connected ...
    if ( ev::Device::ConnectionStatus::Connected == a_status ) {
        // ... ok ...
        return;
    }

    // ... expecting disconnected or error status only ...
    if ( not ( ev::Device::ConnectionStatus::Disconnected == a_status || ev::Device::ConnectionStatus::Error == a_status ) ) {
        // ... ??? ...
        return;
    }

    //
    // ... connection error or device disconnected ....
    //

    // ... sanity check required ...
    SanityCheck();

    // ... search for device ...
    bool found = false;
    for ( auto map : { &cached_devices_, &in_use_devices_ } ) {
        for ( auto it : *map ) {
            const auto device_it = std::find_if(it.second->begin(), it.second->end(), [a_device](const ev::Device* a_it_device) {
                return ( a_it_device == a_device );
            });
            if ( it.second->end() != device_it ) {
                it.second->erase(device_it);
                found = true;
                break;
            }
        }
        if ( true == found ) {
            break;
        }
    }

    // ... finally ...
    if ( false == found ) {
        // ... device not found ...
        ss << "Unable to delete device " << a_device << ", no reference at control maps!";
        // ... report fault ...
        throw ev::Exception(ss.str());
    }

    // ... promote devie to a 'zombie'
    zombies_.insert(a_device);

    // ... search for associated request ...
    const auto r_it = device_request_map_.find(a_device);
    if ( device_request_map_.end() != r_it ) {

        typedef struct {
            const int64_t            invoke_id_;
            const ev::Object::Target target_;
            const uint8_t            tag_;
        } Payload;

        // ... prepare callback payload ...
        Payload* payload = new Payload({r_it->second->GetInvokeID(), r_it->second->target_, r_it->second->GetTag()});

        // ... untrack ...
        Unlink(r_it->second);

        // ... issue callbacks ...
        stepper_.disconnected_->Call(
                                     [this, payload]() -> void* {
                                         CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
                                         return payload;
                                     },
                                     [](void* a_payload, ev::hub::DisconnectedStepCallback a_callback) {
                                         CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                                         Payload* r_payload = static_cast<Payload*>(a_payload);
                                         a_callback(
                                                    /* a_invoke_id */ r_payload->invoke_id_,
                                                    /* a_target    */ r_payload->target_,
                                                    /* a_tag       */ r_payload->tag_
                                                    );
                                         delete r_payload;
                                     }
        );
    }
}

/**
 * @brief This method will be called when a device received a result object and no one collected it.
 *
 * @param a_device
 * @param a_request
 * @param a_result
 *
 * @return True when the object ownership is accepted, otherwise it will be relased the this function caller.
 */
bool ev::hub::OneShotHandler::OnUnhandledDataObjectReceived (const ev::Device* /* a_device */, const ev::Request* /* a_request */, ev::Result* /* a_result */)
{
    // ... this function should not be called for this handler, if so it might be a bug ...
    // ... reject ownership of the data object ...
    return false;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Handle pushed requests.
 */
void ev::hub::OneShotHandler::Push ()
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

    // ... get rid of 'zombies' objects ...
    KillZombies();

    size_t idx = 0;

    // ...
    PurgeDevices();

    // ... now process next request(s) ....
    do {
        // ... no more requests?
        if ( 0 == pending_requests_.size() ) {
            // ... we're done ...
            break;
        }
        // ... peek next request ...
        ev::Request* current_request = pending_requests_[idx];
        // ... sanity check ...
        auto cached_device_for_type_it = cached_devices_.find(current_request->target_);
        if ( cached_devices_.end() == cached_device_for_type_it ) {
            // ... a vector should be ready !
            throw ev::Exception("Unexpected device 'cached' map state: nullptr!");
        }
        auto in_use_device_for_type_it = in_use_devices_.find(current_request->target_);
        if ( in_use_devices_.end() == in_use_device_for_type_it ) {
            // ... a vector should be ready !
            throw ev::Exception("Unexpected device 'in-use' map state: nullptr!");
        }
        const size_t in_use_devices_cnt = in_use_device_for_type_it->second->size();
        // ... limit reached?
        const auto limits_it          = devices_limits_.find(current_request->target_);
        size_t     max_devices_in_use = 2;
        if ( devices_limits_.end() != limits_it ) {
            max_devices_in_use = limits_it->second;
        }
        // ... if limit reached ...
        if ( in_use_devices_cnt >= max_devices_in_use ) {
            // ... give a chance to other kind of requests to be handled ...
            idx++;
            continue;
        }
        // ... remove it ...
        pending_requests_.erase(pending_requests_.begin() + idx);
        ++idx;
        // ... connect device for request ...
        switch (current_request->target_) {

            case ev::Object::Target::Redis:
            case ev::Object::Target::PostgreSQL:
            {
                if ( ev::Request::Control::Invalidate == current_request->control_ ) {
                    // ... first invalidate all connections for specific target ...
                    InvalidateDevices(current_request->target_);
                    // ... then untrack request ...
                    Unlink(current_request);
                    // ... mark as completed ...
                    completed_requests_.push_back(current_request);
                    // ... forget it ...
                    current_request = nullptr;
                    // ... sanity check required ...
                    SanityCheck();
                    // ... publish result now ...
                    Publish();
                    break;
                }
            }
            case ev::Object::Target::CURL:
            {
                ev::Device* device;
                bool        new_device;
                if ( 0 == cached_device_for_type_it->second->size() ) {
                    device     = stepper_.factory_(current_request);
                    new_device = true;
                } else {
                    device     = (*cached_device_for_type_it->second)[0];
                    new_device = false;
                    cached_device_for_type_it->second->erase(cached_device_for_type_it->second->begin());
                }

                // ... ensure deice exists ...
                if ( nullptr == device ) {
                    // ... a device must be ready!
                    throw ev::Exception("Unexpected device 'in-use' map state: nullptr!");
                }

                // ... setup device ...
                stepper_.setup_(device);

                // ... listen to connection status changes ...
                device->SetListener(this);

                Link(current_request, device);

                const ev::Device::Status connect_rv = device->Connect([this, current_request](const ev::Device::ConnectionStatus& a_status, ev::Device* a_device) {

                    bool success = ( ev::Device::ConnectionStatus::Connected == a_status );
                    if ( true == success ) {
                        const ev::Device::Status exec_rv = a_device->Execute(
                                                                             [this, current_request, a_device] (const ev::Device::ExecutionStatus& a_exec_status, ev::Result* a_exec_result) {

                                                                                 CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

                                                                                 (void)a_exec_status;

                                                                                 const auto target = current_request->target_;

                                                                                 Unlink(current_request);

                                                                                 current_request->AttachResult(a_exec_result);

                                                                                 // ... mark request as completed ...
                                                                                 completed_requests_.push_back(current_request);
                                                                                 // ... remove device from 'in use' map ...
                                                                                 auto i_u_it = in_use_devices_.find(target);
                                                                                 if ( in_use_devices_.end() != i_u_it ) {
                                                                                     for ( auto device_it = i_u_it->second->begin(); i_u_it->second->end() != device_it ; device_it ++  ) {
                                                                                         if ( (*device_it) == a_device ) {
                                                                                             i_u_it->second->erase(device_it);
                                                                                             break;
                                                                                         }
                                                                                     }
                                                                                 }

                                                                                 // ... and add it to 'cached' map
                                                                                 if ( false == a_device->Reusable() ) {
                                                                                     // ... device already unlinked ...
                                                                                     // ... we're no longer tracking it ...
                                                                                     // ... it should be deleted after this callback ...
                                                                                     a_device->SetUntracked();
                                                                                 } else {
                                                                                     cached_devices_[target]->push_back(a_device);
                                                                                 }

                                                                                 // ... sanity check required ...
                                                                                 SanityCheck();

                                                                                 // ... publish results now ...
                                                                                 Publish();
                                                                             },
                                                                             current_request
                        );
                        success = ( ev::Device::Status::Async == exec_rv );
                    }

                    OSALITE_DEBUG_TRACE("ev_one_shot_handler",
                                        "{ %d } : [%c] | # %d / %d ",
                                        (int)current_request->target_, a_device->Reusable() ? ' ' : 'x',
                                        (int)a_device->ReuseCount(), (int)a_device->MaxReuse()
                    );

                    if ( false == success ) {

                        const auto target = current_request->target_;

                        auto i_u_it = in_use_devices_.find(target);
                        if ( in_use_devices_.end() != i_u_it ) {
                            for ( auto device_it = i_u_it->second->begin(); i_u_it->second->end() != device_it ; device_it ++  ) {
                                if ( (*device_it) == a_device ) {
                                    i_u_it->second->erase(device_it);
                                    break;
                                }
                            }
                        }

                        ev::Result* result = new ev::Result(target);
                        result->AttachDataObject(a_device->DetachLastError());
                        current_request->AttachResult(result);

                        // ... untrack ...
                        Unlink(current_request);
                        // ... keep track of this request ...
                        rejected_requests_.push_back(current_request);
                        // dont need to current_request = nullptr; we're at another context ...

                        // ... and add it to 'cached' map
                        if ( false == a_device->Reusable() ) {
                            // ... no longer usable ...
                            // ... device already unlinked ...
                            // ... it's safe to delete it now ...
                            delete a_device;
                        } else {
                            // ... keep track of the device ...
                            cached_devices_[target]->push_back(a_device);
                        }

                        // ... sanity check required ...
                        SanityCheck();
                        // ... publish results now ...
                        Publish();
                    }

                });

                if ( ev::Device::Status::Async == connect_rv || ev::Device::Status::Nop == connect_rv ) {
                    // ... keep track of the device ...
                    in_use_devices_[current_request->target_]->push_back(device);
                    // ... sanity check required ...
                    SanityCheck();
                    // ... it's an async request ...
                    continue;
                }

                // ... untrack ...
                Unlink(current_request);

                ev::Result* result = new ev::Result(current_request->target_);
                result->AttachDataObject(device->DetachLastError());
                current_request->AttachResult(result);

                // ... request wont run ...
                if ( true == new_device ) {
                    // ... new device is invalid ...
                    delete device;
                } else {
                    // ... keep track of the device ...
                    cached_devices_[current_request->target_]->push_back(device);
                }

                // ... keep track of this request ...
                rejected_requests_.push_back(current_request);
                current_request = nullptr;
                // ... sanity check required ...
                SanityCheck();
                // ... publish results now ...
                Publish();
                // ... let it exit switch, to process rejected request ....
                break;
            }

            default:
            {
                // ... keep track of this request ...
                rejected_requests_.push_back(current_request);
                current_request = nullptr;
                // ... sanity check required ...
                SanityCheck();
                // ... publish results now ...
                Publish();
                break;
            }
        }

        // ... untrack ...
        Unlink(current_request);

        // ... if no one picked this object ....
        if ( nullptr != current_request ) {
            rejected_requests_.push_back(current_request);
            // ... sanity check required ...
            SanityCheck();
            // ... publish results now ...
            Publish();
        }

        // ... next ...

    } while ( idx < pending_requests_.size() );
    
    // ... publish results now ...
    Publish();
    
}


/**
 * @brief Pull a 'completed' request from this handler, ( it's memory MUST be managed by called of this object ).
 *
 * @param o_request All 'completed' requests.
 */
void ev::hub::OneShotHandler::Publish ()
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);

    // ... get rid of 'zombies' objects ...
    KillZombies();

    // ... if nothing to publish ...
    if ( 0 == ( completed_requests_.size() + rejected_requests_.size() ) ) {
        // ... we're done ...
        return;
    }

    std::deque<ev::Request*>* p_requests = new std::deque<ev::Request*>();

    // ... first check if we have completed requests ...
    if ( completed_requests_.size() > 0 ) {
        while ( completed_requests_.size() > 0 ) {
            (*p_requests).push_back(completed_requests_.front());
            completed_requests_.pop_front();
        }
    }
    // ... now check if we have rejected requests ...
    if ( rejected_requests_.size() > 0 ) {
        while ( rejected_requests_.size() > 0 ) {
            (*p_requests).push_back(rejected_requests_.front());
            rejected_requests_.pop_front();
        }
    }

    stepper_.next_->Call(
                         [this, p_requests] () -> void* {
                             CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
                             return p_requests;
                         },
                         [](void* a_payload, ev::hub::NextStepCallback a_callback) {
                             CC_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();
                             // ... for all requests for a specific handler ...
                             std::deque<ev::Request*>* requests = static_cast<std::deque<ev::Request*>*>(a_payload);
                             while ( requests->size() > 0 ) {
                                 ev::Request* request = requests->front();
                                 ev::Result*  detached_result = request->DetachResult();
                                 // ... we're transfering the 'result' object ( ownership ) ...
                                 if ( false == a_callback(request->GetInvokeID(), request->target_, request->GetTag(), detached_result) ) {
                                     // ... ownership transfer was refused ...
                                     delete detached_result;
                                 }
                                 delete request;
                                 requests->pop_front();
                             }
                             delete requests;
                         }
    );
}

/**
 * @brief Link a \link ev::Request \link to a \link ev::Device \link.
 *
 * @param a_request
 * @param a_device
 */
void ev::hub::OneShotHandler::Link (Request* a_request, Device* a_device)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    request_device_map_[a_request] = a_device;
    device_request_map_[a_device]  = a_request;
    OSALITE_ASSERT(request_device_map_.size() == device_request_map_.size());
}

/**
 * @brief Unlink a \link ev::Request \link from a \link ev::Device \link.
 *
 * @param a_request
 * @param a_device
 */
void ev::hub::OneShotHandler::Unlink (Request* a_request)
{
    CC_DEBUG_FAIL_IF_NOT_AT_THREAD(thread_id_);
    const auto r_it = request_device_map_.find(a_request);
    if ( request_device_map_.end() != r_it ) {
        const auto d_it = device_request_map_.find(r_it->second);
        if ( device_request_map_.end() != d_it ) {
            device_request_map_.erase(d_it);
        }
        request_device_map_.erase(r_it);
    }
    OSALITE_ASSERT(request_device_map_.size() == device_request_map_.size());
}

/**
 * @brief Release 'zombie' objects.
 */
void ev::hub::OneShotHandler::KillZombies ()
{
    for ( auto device : zombies_ ) {
        delete device;
    }
    zombies_.clear();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Invalidate devices for a specific target.
 *
 * @param a_target
 */
void ev::hub::OneShotHandler::InvalidateDevices (const ev::Object::Target a_target)
{
    const auto cached_it = cached_devices_.find(a_target);
    if ( cached_devices_.end() != cached_it ) {
        for ( auto device_it = (*cached_it->second).begin(); device_it != (*cached_it->second).end(); ++device_it ) {
            (*device_it)->InvalidateReuse();
        }
    }
    const auto in_use_it = in_use_devices_.find(a_target);
    if ( in_use_devices_.end() != in_use_it ) {
        for ( auto device_it = (*in_use_it->second).begin(); device_it != (*in_use_it->second).end(); ++device_it ) {
            (*device_it)->InvalidateReuse();
        }
    }
    PurgeDevices();
}

/**
 * @brief Iterate all cached devices and delete them.
 */
void ev::hub::OneShotHandler::PurgeDevices ()
{
    for ( auto it : cached_devices_ ) {
        for ( auto device_it = (*it.second).begin(); device_it != (*it.second).end(); ) {
            if ( false == (*device_it)->Reusable() ) {
                delete (*device_it);
                device_it = (*it.second).erase(device_it);
            } else {
                ++device_it;
            }
        }
    }
}
