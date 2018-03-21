/**
 * @file task.cc
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

#include "ev/scheduler/task.h"

#include "osal/osalite.h"

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 *
 * @param a_callback
 * @param a_commit_callback
 */
ev::scheduler::Task::Task (const EV_TASK_PARAMS& a_callback, const EV_TASK_COMMIT_CALLBACK& a_commit_callback)
    : ev::scheduler::Object(ev::scheduler::Object::Type::Task)
{
    first_               = a_callback;
    commit_callback_     = a_commit_callback;
    catch_callback_      = nullptr;
    step_                = -1;
    previous_result_     = nullptr;
}

/**
 * @brief Destructor.
 */
ev::scheduler::Task::~Task ()
{
    first_           = nullptr;
    commit_callback_ = nullptr;
    catch_callback_  = nullptr;
    last_            = nullptr;
    sequences_.clear();
    if ( nullptr != previous_result_ ) {
        delete previous_result_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Perform next action in sequence.
 *
 * @param a_object  Previous step result.
 * @param o_request The next request to be performed, nullptr if none.
 *
 * @return True if this object can be release, false otherwise.
 */
bool ev::scheduler::Task::Step (ev::Object* a_object, ev::Request** o_request)
{
    OSALITE_DEBUG_FAIL_IF_NOT_AT_MAIN_THREAD();

    (*o_request) = nullptr;

    ev::Object* next = nullptr;
    
    const auto report_exception = [this, a_object, next] (const ev::Exception& a_ev_exception) {
        if ( nullptr != catch_callback_ ) {
            catch_callback_(a_ev_exception);
        }
        if ( nullptr != a_object && next != a_object ) {
            delete a_object;
        }
    };
    
    try {
        if ( -1 == step_ ) {
            next = first_();
            step_++;
        } else if ( step_ < static_cast<ssize_t>(sequences_.size()) ) {
            next = sequences_[step_](a_object);
            step_++;
        } else if ( static_cast<ssize_t>(sequences_.size()) == step_ ) {
            if ( nullptr != last_ ) {
                last_(a_object);
            }
            step_++;
        }
    } catch (const ev::Exception& a_ev_exception) {
        report_exception(a_ev_exception);
        // ... task should be released ...
        return true;
    } catch (const std::bad_alloc& a_bad_alloc) {
        OSALITE_BACKTRACE();
        report_exception(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
        // ... task should be released ...
        return true;
    } catch (const std::runtime_error& a_rte) {
        OSALITE_BACKTRACE();
        report_exception(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
        // ... task should be released ...
        return true;
    } catch (const std::exception& a_std_exception) {
        report_exception(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
        // ... task should be released ...
        return true;
    } catch (...) {
        OSALITE_BACKTRACE();
        report_exception(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
        // ... task should be released ...
        return true;
    }
    
    // ... release a_object, now?
    if ( nullptr != a_object && next != a_object ) {
        delete a_object;
    }
    
    // ... next is a new request?
    if ( nullptr != next && ev::Object::Type::Request == next->type_ ) {
        (*o_request) = static_cast<ev::Request*>(next);
        return ( nullptr == (*o_request) );
    }
    
    // ... next, deliver results ...
    if ( step_ <= static_cast<ssize_t>(sequences_.size()) )  {
        if ( nullptr != next ) {
            if ( ev::Object::Type::Result == next->type_ || ev::Object::Type::Reply == next->type_ ) {
                return Step(next, o_request);
            } else {
                delete next;
                if ( nullptr != catch_callback_ ) {
                    catch_callback_(ev::Exception("Can't perform task next step - invalid state!"));
                }
                // ... task should be released ...
                return true;
            }
            return Step(next, o_request);
        } else {
            return Step(nullptr, o_request);
        }
    }
    
    // ... task should be released ...
    return true;
}

/**
 * @brief Called when a connection to a task request was closed.
 *
 * @return \li True if this object is no longer required.
 *         \li False if this object must be kept alive.
 */
bool ev::scheduler::Task::Disconnected ()
{
    return true;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Chain callbacks in task.
 *
 * @param a_callback
 */
ev::scheduler::Task* ev::scheduler::Task::Then (const EV_TASK_CALLBACK& a_callback)
{
    if ( nullptr != last_ ) {
        throw ev::Exception("Task chain already finalized!");
    }
    sequences_.push_back(a_callback);
    return this;
}

/**
 * @brief Set final callbackk
 *
 * @param a_callback
 */
ev::scheduler::Task* ev::scheduler::Task::Finally (const EV_TASK_FINALLY_CALLBACK& a_callback)
{
    last_ = a_callback;
    return this;
}

/**
 * @brief Set catch handler.
 *
 * @param a_callback
 */
void ev::scheduler::Task::Catch (const EV_TASK_CATCH_CALLBACK &a_callback)
{
    catch_callback_ = a_callback;
    try {
        commit_callback_(this);
    } catch (const ev::Exception& a_ev_exception) {
        if ( nullptr != catch_callback_ ) {
            catch_callback_(a_ev_exception);
        }
    } catch (const std::bad_alloc& a_bad_alloc) {
        if ( nullptr != catch_callback_ ) {
            catch_callback_(ev::Exception("C++ Bad Alloc: %s\n", a_bad_alloc.what()));
        }
    } catch (const std::runtime_error& a_rte) {
        if ( nullptr != catch_callback_ ) {
            catch_callback_(ev::Exception("C++ Runtime Error: %s\n", a_rte.what()));
        }
    } catch (const std::exception& a_std_exception) {
        if ( nullptr != catch_callback_ ) {
            catch_callback_(ev::Exception("C++ Standard Exception: %s\n", a_std_exception.what()));
        }
    } catch (...) {
        OSALITE_BACKTRACE();
        if ( nullptr != catch_callback_ ) {
            catch_callback_(ev::Exception(STD_CPP_GENERIC_EXCEPTION_TRACE()));
        }
    }
}
