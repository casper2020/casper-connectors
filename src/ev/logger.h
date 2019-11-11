/**
 * @file logger.h
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
#ifndef NRS_EV_LOGGER_H_
#define NRS_EV_LOGGER_H_

#include "ev/loggable.h"

#include "osal/osal_singleton.h"
#include "cc/utc_time.h"

#include <exception>
#include <string>
#include <map>
#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h> // chmod
#include <unistd.h> // getpid, chown
#include <mutex> // std::mutext, std::lock_guard
#include <string.h> // stderror

namespace ev
{
    
    /**
     * @brief A singleton to log messages.
     */
    class Logger final : public osal::Singleton<Logger>
    {
        
    private: // Threading
        
        std::mutex mutex_;
        
    public: // Data Types
        
        struct OutOfMemoryException : std::exception {
            const char* what() const noexcept {return "Out Of Memory!";}
        };
        
        class RegistrationException : public std::exception {
            
        private: // Data
            
            const std::string what_;
            
        public: // Constructor / Destructor
            
            RegistrationException (const std::string& a_what)
                : what_(a_what)
            {
                // ... empty ...
            }
            
            virtual ~RegistrationException ()
            {
                // ... empty ...
            }
            
        public:
            
            const char* what() const noexcept {return what_.c_str();}
        };
        
    protected: // Data Types
        
        /**
         * An object that defines a token.
         */
        class Token
        {
            
        public: // Const Data
            
            const std::string name_;
            const std::string fn_;
            
        public: // Data
            
            FILE* fp_;
            
        public: // Constructor(s) / Destructor
            
            /**
             * @brief Default constructor.
             *
             * @param a_name The token name.
             * @param a_fn
             * @param a_fp
             */
            Token (const std::string& a_name, const std::string& a_fn, FILE* a_fp)
                : name_(a_name), fn_(a_fn), fp_(a_fp)
            {
                /* empty */
            }
            
            /**
             * @brief Destructor.
             */
            virtual ~Token ()
            {
                if ( nullptr != fp_ && stderr != fp_ && stdout != fp_ ) {
                    fclose(fp_);
                }
            }
            
        };
        
    private: // Data
        
        std::map<std::string, Token*> tokens_;
        char*                         buffer_;
        size_t                        buffer_capacity_;
        uid_t                         user_id_;
        gid_t                         group_id_;
        
    public: // Initialization / Release API - Method(s) / Function(s)
        
        void    Startup  ();
        void    Shutdown ();
        
    public: // Registration API - Method(s) / Function(s)
        
        void     Register     (const std::string& a_token, const std::string& a_file);
        bool     IsRegistered (const char* const a_token);
        
    public: // Log API - Method(s) / Function(s)
        
        void     Log (const std::string& a_token, const Loggable::Data& a_data, const char* a_format, ...) __attribute__((format(printf, 4, 5)));
        
    public: // Other - Method(s) / Function(s)
        
        void     Recycle     ();
        bool     EnsureOwner (uid_t a_user_id, gid_t a_group_id);
        
    private: //
        
        bool     IsRegistered         (const std::string& a_token) const;
        bool     EnsureBufferCapacity (const size_t& a_capacity);
        bool     EnsureOwner          ();
        
    }; // end of class Logger
    
    /**
     * @brief Initialize logger instance.
     *
     * @param a_name
     */
    inline void Logger::Startup  ()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_          = new char[1024];
        buffer_capacity_ = nullptr != buffer_ ? 1024 : 0;
        user_id_         = UINT32_MAX;
        group_id_        = UINT32_MAX;
    }
    
    /**
     * @brief Release all dynamically allocated memory
     */
    inline void Logger::Shutdown ()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for ( auto it : tokens_ ) {
            delete it.second;
        }
        tokens_.clear();
        if ( nullptr != buffer_ ) {
            delete [] buffer_;
            buffer_ = nullptr;
        }
    }
    
    /**
     * @brief Register a token.
     *
     * @param a_token The token name.
     */
    inline void Logger::Register (const std::string& a_token, const std::string& a_file)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... already registered? ...
        if ( tokens_.end() != tokens_.find(a_token) ) {
            return;
        }
        // ... try to open log file ...
        FILE* fp = fopen(a_file.c_str(), "a");
        if ( nullptr == fp ) {
            const char* const err_str = strerror(errno);
            throw RegistrationException("An error occurred while creating preparing log file '" + a_file + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !");
        }
        // ... keep track of it ...
        try {
            tokens_[a_token] = new Token(a_token, a_file, fp);
        } catch (...) {
            // ... failure ...
            throw RegistrationException("A 'C++ Generic Exception' occurred while registering log token '" + a_token + "'!");
        }
    }
    
    /**
     * @brief Check if a token is already registered.
     *
     * @param a_token The token name.
     *
     * @return True when the token is already registered.
     */
    inline bool Logger::IsRegistered (const char* const a_token)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return tokens_.end() != tokens_.find(a_token);
    }
    
    /**
     * @brief Output a log message if the provided token is registered.
     *
     * @param a_token The token to be tested.
     * @param a_data
     * @param a_format
     * @param ...
     */
    inline void Logger::Log (const std::string& a_token, const Loggable::Data& a_data, const char* a_format, ...)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ...if token is not registered...
        if ( false == IsRegistered(a_token) ) {
            // ... we're done ...
            return;
        }
        
        // ... ensure we have a valid buffer ...
        if ( false == EnsureBufferCapacity(1024) ) {
            // ... oops ...
            return;
        }
        
        // ... logger ...
        std::string module_name;
        if ( strlen(a_data.module()) > 22 ) {
            module_name = "..." + std::string(a_data.module() + ( strlen(a_data.module()) + 3 - 22));
        } else {
            module_name = a_data.module();
        }
        
        char prefix [256] = { 0 };
        snprintf(prefix, sizeof(prefix) / sizeof(prefix[0]),
                 "%8u, %15.15s, %22.22s, %32.32s, %p, ",
                 static_cast<unsigned>(getpid()),
                 a_data.ip_addr(),
                 module_name.c_str(),
                 a_data.tag(),
                 a_data.owner_ptr()
        );
        
        int aux = INT_MAX;
        
        // ... try at least 2 times to construct the output message ...
        for ( uint8_t attempt = 0 ; attempt < 2 ; ++attempt ) {
            
            va_list args;
            va_start(args, a_format);
            aux = vsnprintf(buffer_, buffer_capacity_ - 1, a_format, args);
            va_end(args);
            
            if ( aux < 0 ) {
                // ... an error has occurred ...
                break;
            } else if ( aux > static_cast<int>(buffer_capacity_) ) {
                // ... realloc buffer ...
                if ( true == EnsureBufferCapacity(static_cast<size_t>(aux + 1)) ) {
                    // ... last attempt to write to buffer ...
                    continue;
                } else {
                    // ... out of memory ...
                    break;
                }
            } else {
                // ... all done ...
                break;
            }
        }
        
        // ... ready to output the message ? ...
        if ( aux > 0 && static_cast<size_t>(aux) < buffer_capacity_ ) {
            auto file = tokens_.find(a_token)->second->fp_;
            // ... output message ...
            fprintf(tokens_.find(a_token)->second->fp_, "%s,%s",
                    cc::UTCTime::NowISO8601WithTZ().c_str(), prefix
            );
            fprintf(tokens_.find(a_token)->second->fp_, "%s\n", buffer_);
            // ... flush ...
            if ( stdout != file && stderr != file ) {
                fflush(file);
            }
        }
    }
    
    
    /**
     * @brief Re-open log files ( if any ).
     */
    inline void Logger::Recycle ()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... for all tokens ...
        for ( auto it : tokens_ ) {
            // ... if no file is open ...
            if ( stdout == it.second->fp_ || stderr == it.second->fp_ ) {
                // ... next ...
                continue;
            }
            // ... else close ...
            if ( nullptr != it.second->fp_ ) {
                fclose(it.second->fp_);
            }
            // ... and reopen it ...
            it.second->fp_ = fopen(it.second->fn_.c_str(), "w");
            if ( nullptr == it.second->fp_ ) {
                const char* const err_str = strerror(errno);
                throw RegistrationException("An error occurred while creating rotating log file '" + it.second->fn_ + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !");
            }
            // ... write a comment line ...
            fprintf(it.second->fp_, "---- NEW LOG '%s' ----\n", it.second->fn_.c_str());
            fflush(it.second->fp_);
        }
        EnsureOwner();
    }
    
    /**
     * @brief Change the logs permissions to a specific user / group.
     *
     * @param a_user_id
     * @param a_group_id
     *
     * @return True if all files changed to new permissions or it not needed, false otherwise.
     */
    inline bool Logger::EnsureOwner (uid_t a_user_id, gid_t a_group_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        user_id_  = a_user_id;
        group_id_ = a_group_id;
        return EnsureOwner();
    }
    
    /**
     * @brief Check if a token is already registered.
     *
     * @param a_token The token name.
     *
     * @return True when the token is already registered.
     */
    inline bool Logger::IsRegistered (const std::string& a_token) const
    {
        return tokens_.end() != tokens_.find(a_token);
    }
    
    /**
     * @brief Ensure that the current buffer has at least the required capacity.
     *
     * @param a_capacity The required capacity ( in bytes ).
     *
     * @return True when the buffer has at least the required capacity.
     */
    inline bool Logger::EnsureBufferCapacity (const size_t& a_capacity)
    {
        // ... buffer is allocated and has enough capacity? ...
        if ( buffer_capacity_ >= a_capacity ) {
            // ... good to go ...
            return true;
        }
        // ... if buffer is not allocated ...
        if ( nullptr != buffer_ ) {
            delete [] buffer_;
        }
        // ... try to allocate it now ...
        buffer_ = new char[a_capacity];
        if ( nullptr == buffer_ ) {
            // ... out of memory ...
            buffer_capacity_ = 0;
        } else {
            // ... we're good to go ....
            buffer_capacity_ = a_capacity;
        }
        // ... we're good to go if ...
        return buffer_capacity_ == a_capacity;
    }
    
    /**
     * @brief Change the logs permissions to a specific user / group.
     *
     * @return True if all files changed to new permissions or it not needed, false otherwise.
     */
    inline bool Logger::EnsureOwner ()
    {
        if ( UINT32_MAX == user_id_ || UINT32_MAX == group_id_ ) {
            return true;
        }
        size_t count = 0;
        for ( auto it : tokens_ ) {
            const int chown_status = chown(it.second->fn_.c_str(), user_id_, group_id_);
            const int chmod_status = chmod(it.second->fn_.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if ( 0 == chown_status && 0 == chmod_status ) {
                count++;
            }
        }
        return ( tokens_.size() == count );
    }
    
} // end of namespace 'ev'

#endif // NRS_EV_LOGGER_H_
