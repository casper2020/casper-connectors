/**
 * @file process.h
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

#ifndef NRS_SYS_PROCESS_H_
#define NRS_SYS_PROCESS_H_
#pragma once

#include <map>
#include <list>
#include <vector>
#include <string>
#include <list>
#include <sys/types.h> // pid_t

#include "sys/error.h"

#include "cc/non-copyable.h"
#include "cc/non-movable.h"

namespace sys
{
    
    class Process : public ::cc::NonCopyable, public ::cc::NonMovable
    {
        
    public: // Data Type(s)
        
        class Info
        {
            
        public: // Const Data
            
            const std::string              id_;
            const std::string              owner_;
            const std::string              path_;
            const std::string              executable_;
            const std::string              arguments_;
            const std::string              user_;
            const std::string              group_;
            const std::string              working_dir_;
            const std::string              log_dir_;
            const std::string              pid_file_;
            const std::vector<std::string> depends_on_;
            
        public: // Constructor(s) / Destructor
            
            Info (const std::string& a_id, const std::string& a_owner,
                  const std::string& a_path, const std::string& a_executable, const std::string& a_arguments,
                  const std::string& a_user, const std::string& a_group,
                  const std::string& a_working_dir, const std::string& a_log_dir,
                  const std::string& a_pid_file,
                  const std::list<std::string>& a_depends_on)
            : id_(a_id), owner_(a_owner),
                path_(a_path), executable_(a_executable), arguments_(a_arguments),
                user_(a_user), group_(a_group),
                working_dir_(a_working_dir), log_dir_(a_log_dir),
                pid_file_(a_pid_file),
                depends_on_(a_depends_on.begin(), a_depends_on.end())
            {
                /* empty */
            }
            
            Info (const Info& a_info)
            : id_(a_info.id_), owner_(a_info.owner_),
                path_(a_info.path_), executable_(a_info.executable_), arguments_(a_info.arguments_),
                user_(a_info.user_), group_(a_info.group_),
                working_dir_(a_info.working_dir_), log_dir_(a_info.log_dir_),
                pid_file_(a_info.pid_file_),
                depends_on_(a_info.depends_on_.begin(), a_info.depends_on_.end())
            {
                return;
            }
            
            Info (Info && a_info)
            : id_(std::move(a_info.id_)), owner_(std::move(a_info.owner_)),
                path_(std::move(a_info.path_)), executable_(std::move(a_info.executable_)), arguments_(std::move(a_info.arguments_)),
                user_(std::move(a_info.user_)), group_(std::move(a_info.group_)),
                working_dir_(std::move(a_info.working_dir_)), log_dir_(std::move(a_info.log_dir_)),
                pid_file_(std::move(a_info.pid_file_)),
                depends_on_(std::move(a_info.depends_on_))
            {
                return;
            }
            
            virtual ~Info ()
            {
                /* empty */
            }
            
        public: // NOT ALLOWED
            
            Info ()                         = delete; //!< Default Constructor
            Info& operator = (Info const&) = delete; //!< Copy Assign
            Info& operator = (Info &&)     = delete; //!< Move Assign
            
        }; // end of class 'Info';
        
        typedef std::list<Process*> List;
        
    protected: // Data
        
        Info          info_;
        
        pid_t         pid_;
        std::string   uri_;
        char**        argv_;
        size_t        argc_;
        
        Error         error_;
        
    public: // NOT ALLOWED
        
        Process() = delete; //!< Default Constructor
        
    public: // Constructor(s) / Destructor
        
        Process (Info&& a_info);
        virtual ~Process ();
        
    public: // Operator(s)
        
        inline void operator = (const std::string& a_uri)
        {
            uri_ = a_uri;
        }
        
        inline void operator = (const std::vector<std::string>& a_args)
        {
            if ( nullptr != argv_ ) {
                for ( size_t idx = 0 ; idx < sizeof(argv_) / sizeof(argv_[0]) ; ++idx ) {
                    if ( nullptr != argv_[idx] ) {
                        free(argv_[idx]);
                    }
                }
                free(argv_);
            }
            argc_ = a_args.size() + 1;
            if ( argc_ > 0 ) {
                argv_    = (char**)malloc(( argc_ + 1 )  * sizeof(char*));
                if ( info_.owner_.length() > 0 ) {
                    argv_[0] = strdup(( "[" + info_.owner_ + "] " + info_.executable_).c_str());
                } else {
                    argv_[0] = strdup(info_.executable_.c_str());
                }
                for ( size_t idx = 0 ; idx < ( argc_ - 1 ) ; ++idx ) {
                    argv_[idx + 1] = strdup(a_args[idx].c_str());
                }
                argv_[argc_] = nullptr;
            } else {
                argv_ = nullptr;
            }
        }
        
        inline void operator = (const pid_t& a_pid)
        {
            pid_ = a_pid;
        }
        
    public: // Method(s) / Function(s)
        
        const pid_t&       pid          () const;
        const std::string& uri          () const;
        const size_t&      argc         () const;
        char** const&      argv         () const;
        const Info&        info         () const;
        const Error&       error        () const;
        bool               is_error_set () const;
        
    public:
        
        bool Signal    (const int a_signal_no, const bool a_optional);
        bool Kill      (const bool a_optional);
        bool Terminate (const bool a_optional);
        
        bool WritePID        ();
        bool ReadPID         (const bool a_optional, pid_t& o_pid);
        bool UnlinkPID       (const bool a_optional);
        bool LoadPIDFromFile (const bool a_optional);
        
    public: // Pure Virtual Method(s) / Function(s) - Declaration
        
        virtual bool IsZombie  (const bool a_optional, bool& o_is_zombie) = 0;
        virtual bool IsRunning (const bool a_optional, const pid_t a_parent_pid, bool& o_is_running) = 0;
        
    public: // Static Method(s) / Function(s) - Declaration
        
        static bool Filter (const std::list<const Process*>& a_interest, std::list<const Process*>& o_list);
        static void Sort   (const std::vector<const Process::Info>& a_vector, std::list<const Process::Info>& o_list);
        
    protected:
        
        void SetError (const Process* a_process,
                       const char* const /* a_file */, const char* const a_function, const int a_line,
                       const errno_t a_errno, const char* const a_format, ...)  __attribute__((format(printf, 7, 8)));
        
    }; // end of class 'Process'
    
    inline const pid_t& Process::pid () const
    {
        return pid_;
    }
    
    inline const std::string& Process::uri () const
    {
        return uri_;
    }
    
    inline const size_t& Process::argc () const
    {
        return argc_;
    }
    
    inline char** const& Process::argv () const
    {
        return argv_;
    }
    
    inline const Process::Info& Process::info () const
    {
        return info_;
    }
    
    inline const Error& Process::error () const
    {
        return error_;
    }
    
    inline bool Process::is_error_set () const
    {
        return ( 0 != error_.message().length() );
    }
    
} // end of namespace 'sys'

#endif // NRS_SYS_PROCESS_H_

