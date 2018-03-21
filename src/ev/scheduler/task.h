/**
 * @file task.h
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
#ifndef NRS_EV_SCHEDULER_TASK_H_
#define NRS_EV_SCHEDULER_TASK_H_

#include <functional> // std::function
#include <vector>     // std::vector
#include <queue>      // std::queue

#include "ev/object.h"
#include "ev/exception.h"

#include "ev/scheduler/object.h"

namespace ev
{
    
    namespace scheduler
    {

        class Task final : public ev::scheduler::Object
        {
            
        public:
            
#ifndef EV_TASK_PARAMS
#define EV_TASK_PARAMS std::function<::ev::Object*()>
#endif
            
#ifndef EV_TASK_CALLBACK
#define EV_TASK_CALLBACK std::function<ev::Object*(::ev::Object* a_object)>
#endif
            
#ifndef EV_TASK_FINALLY_CALLBACK
#define EV_TASK_FINALLY_CALLBACK std::function<void(::ev::Object* a_object)>
#endif
            
#ifndef EV_TASK_CATCH_CALLBACK
#define EV_TASK_CATCH_CALLBACK std::function<void(const ::ev::Exception& a_exception)>
#endif
            
#ifndef EV_TASK_COMMIT_CALLBACK
#define EV_TASK_COMMIT_CALLBACK std::function<void(::ev::scheduler::Task* a_task)>
#endif
            
#ifndef EV_TASK_STEP_CALLBACK
#define EV_TASK_STEP_CALLBACK std::function<void(const ::ev::Object* a_object, bool a_completed)>
#endif
            
        protected: // Data
            
            EV_TASK_PARAMS                first_;           //!< First callback.
            EV_TASK_FINALLY_CALLBACK      last_;            //!< Last callback.
            std::vector<EV_TASK_CALLBACK> sequences_;       //!< Intermediary callbacks.
            EV_TASK_CATCH_CALLBACK        catch_callback_;  //!< Function to call when an \link ev::Exception \link was caught!
            EV_TASK_COMMIT_CALLBACK       commit_callback_; //!< Mandatory callback, to finalize task setup.
            ssize_t                       step_;            //!< Current task step.
            ev::Result*                   previous_result_; //!< The previously collected result, nullptr if none.
            
        public: // Constructor(s) / Destructor
            
            Task(const EV_TASK_PARAMS& a_callback, const EV_TASK_COMMIT_CALLBACK& a_commit_callback);
            virtual ~Task ();
            
        public: // Inherited Virtual Method(s) / Function(s)
            
            virtual bool Step         (ev::Object* a_object, ev::Request** o_request);
            virtual bool Disconnected ();
            
        public: // Method(s) / Function(s)
            
            Task* Then    (const EV_TASK_CALLBACK& a_callback);
            Task* Finally (const EV_TASK_FINALLY_CALLBACK& a_callback);
            void  Catch   (const EV_TASK_CATCH_CALLBACK& a_callback);
            
        }; // end of class 'Task'

    } // end of namespace 'scheduler'
    
} // end of namespace 'ev'

#endif // NRS_EV_SCHEDULER_TASK_H_
