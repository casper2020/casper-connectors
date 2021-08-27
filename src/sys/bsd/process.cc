/**
 * @file process.cc
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

#include "sys/bsd/process.h"

#include <unistd.h>        // access, pid_t, getppid
#include <errno.h>         // errno
#include <sys/sysctl.h>    // sysctl
#include<mach/mach.h>

#include <exception>


/**
 * @brief Destructor
 */
sys::bsd::Process::Process (sys::Process::Info&& a_info)
    : ::sys::Process(sys::Process::Info(a_info))
{
    /* empty */
}

/**
 * @brief Destructor
 */
sys::bsd::Process::~Process ()
{
    /* empty */
}

/**
 * @brief Check if a process is running and it's a zombie.
 *
 * @param a_optional   When true no errors are reported.
 *
 * @param o_is_zombie True if the process is a zombie, false otherwise.
 */
bool sys::bsd::Process::IsZombie (const bool a_optional, bool& o_is_zombie)
{
    struct proc_bsdshortinfo short_info;

    if ( true == GetInfo(a_optional, short_info) ) {
        o_is_zombie = ( SZOMB == short_info.pbsi_status );
    } else {
        o_is_zombie = false;
    }
    
    // ... if it's a zombie and ...
    if ( 0 == pid_ ) {
        pid_ = short_info.pbsi_pid;
    }
    
    // ... done ...
    return ( false == is_error_set() );
}

/**
 * @brief Check if this process is running.
 *
 * @param a_optional   When true no errors are reported.
 * @param a_parent_pid The parent pid.
 *
 * @param o_is_running True if the process is a running, false otherwise.
 */
bool sys::bsd::Process::IsRunning (const bool a_optional, const pid_t a_parent_pid, bool& o_is_running)
{
    struct proc_bsdshortinfo short_info;

    if ( true == GetInfo(a_optional, short_info) ) {
        o_is_running = ( a_parent_pid == short_info.pbsi_ppid );
    } else {
        o_is_running = false;
    }

    // ... if it's running and ...
    if ( 0 == pid_ ) {
        pid_ = short_info.pbsi_pid;
    }

    // ... done ...
    return ( false == is_error_set() );
}

/**
 * @brief Get the list of running processes 'kill'.
 *
 * @param a_interest
 * @param o_list
 *
 * @return True on success, false otherwise and an error should be set.
 */
bool sys::Process::Filter (const std::list<const sys::Process*>& a_interest,
                           std::list<const sys::Process*>& o_list)
{
    static const int   mib[]    = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    static const u_int mib_size = ( sizeof(mib) / sizeof(*mib) ) - 1;
    struct kinfo_proc* list     = nullptr;
    int                err_no   = 0;
    size_t             length   = 0;
    
    const size_t max_attempts = 3;
    for ( size_t attempt_no = 0 ; attempt_no < max_attempts ; ++attempt_no ) {
        
        // ... first sysctl call is to calculate buffer size ...
        length = 0;
        err_no = sysctl((int *)mib, mib_size, NULL, &length, NULL, 0);
        if ( -1 == err_no ) {
            err_no = errno;
            if ( ENOMEM == err_no ) {
                err_no = 0;
                continue;
            } else {
                break;
            }
        }
        
        // ... allocate an buffer so we make a second call and fill the list ...
        list = (kinfo_proc*)malloc(length);
        if ( list == NULL ) {
            err_no = ENOMEM;
            continue;
        }
        
        // ... obtain processes list...
        // ⚠️ ( since we're making 2 calls in-between we might miss processes ) ⚠️
        if ( -1 == sysctl( (int *) mib, mib_size, list, &length, NULL, 0) ) {
            free(list);
            list = NULL;
            err_no = errno;
            if ( ENOMEM == err_no ) {
                err_no = 0;
                continue;
            } else {
                break;
            }
        } else {
            break;
        }
        
    }
    
    errno = err_no;
    
    // ... an error is set or no data?
    if ( 0 != err_no ) { // ... error is set ...
        free(list);
        return false;
    } else if ( nullptr == list ) { // ... no data ...
        return false;
    }
    
    std::map<std::string, const sys::Process*> interest;
    for ( const auto it : a_interest ) {
        interest[it->info_.executable_] = it;
    }
    
    // ... collect processes that still have the same pid created previously by this monitor ...
    const size_t count  = length / sizeof(kinfo_proc);
    for ( size_t idx = 0 ; idx < count ; ++idx ) {
        const auto process = &list[idx];
        const auto it      = interest.find(process->kp_proc.p_comm);
        if ( interest.end() == it ) {
            continue;
        }
        if ( o_list.end() == std::find(o_list.begin(), o_list.end(), it->second) ) {
            o_list.push_back(it->second);
        }
    }
    free(list);
    
    // ... we're done ..
    return true;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Retrieve a this process info.
 *
 * @para o_info Short info about this process.
 *
 * @return True on success, false on failure.
 */
bool sys::bsd::Process::GetInfo (const bool a_optional, struct proc_bsdshortinfo& o_info)
{
    if ( 0 == pid_ ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  errno,
                  "failed to a signal to '%s', pid not set!", info().id_.c_str()
        );
        return false;
    }
    
    const int rv = proc_pidinfo(pid_, PROC_PIDT_SHORTBSDINFO, /*arg is UNUSED with this flavor*/ 0ULL, &o_info, sizeof(o_info));
    if ( rv != sizeof(o_info) ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  ::sys::Error::k_no_error_,
                  "failed to obtain process info for '%s' with pid %d!", info_.id_.c_str(), pid_
        );
    } else if ( ( 0 == o_info.pbsi_comm[0] || 0 != strcasecmp(o_info.pbsi_comm, info_.executable_.c_str()) ) ) {
        SetError (this, __FILE__, __FUNCTION__, __LINE__,
                  ::sys::Error::k_no_error_,
                  "pid %d does not match process name '%s'", pid_, info_.id_.c_str()
        );
    }
    
    // ... if an error is set and it's optional ....
    if ( true == is_error_set() && true == a_optional ) {
        // ... forget it ...
        error_.Reset();
    }
    
    // ... done ...
    return ( false == is_error_set() );
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Check if current process is being debugged.
 *
 * @param a_pid Process pid.
 *
 * @return True if so, false otherwise.
 */
bool sys::bsd::Process::IsProcessBeingDebugged (const pid_t& a_pid)
{
    struct kinfo_proc   info;
    size_t              size = sizeof(struct kinfo_proc);
    
    memset(&info, 0, size);
    
    // ... tell sysctl the info we want - information about a specific process ...
    int mib[4] = { CTL_KERN,  KERN_PROC, KERN_PROC_PID , a_pid };
    
    // ... grab process info ...
    int err_no = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    if ( -1 == err_no ) {
        err_no = errno;
        throw std::runtime_error("Unable to get process info: " + std::to_string(err_no) + " - " + strerror(err_no));
    }
    
    // ... done - the provided process is being debugged if the P_TRACED flag is set ...
    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

/**
 * @brief Obtain memory physical footprint size for a process by pid.
 *
 * @param a_pid Process pid.
 *
 * @return Memory physical footprint for the provided process id.
 */
ssize_t sys::bsd::Process::MemPhysicalFootprint (const pid_t& a_pid)
{
    task_t task;
    
    kern_return_t error = task_for_pid(mach_task_self(), a_pid, &task);
    if ( KERN_SUCCESS != error ) {
        return -1;
    }
    task_vm_info_data_t vm_info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    error = task_info(task, TASK_VM_INFO, (task_info_t) &vm_info, &count);
    if ( KERN_SUCCESS != error ) {
        return -1;
    }
    return static_cast<ssize_t>(vm_info.phys_footprint);
}
