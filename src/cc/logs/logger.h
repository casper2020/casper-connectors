/**
 * @file logger.h
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
 * casper-connectors  is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_LOGS_LOGGER_H_
#define NRS_CC_LOGS_LOGGER_H_

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h> // chmod
#include <unistd.h>   // getpid, chown
#include <mutex>      // std::mutext, std::lock_guard
#include <string.h>   // stderror
#include <map>        // std::map

#include "cc/utc_time.h"

#if defined(__APPLE__) || defined(__linux__)

    #ifdef __APPLE__
        #define CC_LOGS_LOGGER_COLOR_PREFIX "\033"
    #else // linux
        #define CC_LOGS_LOGGER_COLOR_PREFIX "\e"
    #endif

    #define CC_LOGS_LOGGER_RESET_ATTRS        CC_LOGS_LOGGER_COLOR_PREFIX "[0m"

    #define CC_LOGS_LOGGER_RED_COLOR          CC_LOGS_LOGGER_COLOR_PREFIX "[00;31m"
    #define CC_LOGS_LOGGER_GREEN_COLOR        CC_LOGS_LOGGER_COLOR_PREFIX "[01;32m"
    
    #define CC_LOGS_LOGGER_CYAN_COLOR         CC_LOGS_LOGGER_COLOR_PREFIX "[01;36m"
    #define CC_LOGS_LOGGER_LIGHT_CYAN_COLOR   CC_LOGS_LOGGER_COLOR_PREFIX "[01;96m"

    #define CC_LOGS_LOGGER_BLUE_COLOR         CC_LOGS_LOGGER_COLOR_PREFIX "[01;34m"
    #define CC_LOGS_LOGGER_LIGHT_BLUE_COLOR   CC_LOGS_LOGGER_COLOR_PREFIX "[01;94m"
    
    #define CC_LOGS_LOGGER_LIGHT_GRAY_COLOR   CC_LOGS_LOGGER_COLOR_PREFIX "[01;37m"
    #define CC_LOGS_LOGGER_DARK_GRAY_COLOR    CC_LOGS_LOGGER_COLOR_PREFIX "[01;90m"
    
    #define CC_LOGS_LOGGER_WHITE_COLOR        CC_LOGS_LOGGER_COLOR_PREFIX "[01;97m"
    #define CC_LOGS_LOGGER_YELLOW_COLOR       CC_LOGS_LOGGER_COLOR_PREFIX "[0;33m"
    #define CC_LOGS_LOGGER_ORANGE_COLOR       CC_LOGS_LOGGER_COLOR_PREFIX "[01;33m"
    
    #define CC_LOGS_LOGGER_WARNING_COLOR      CC_LOGS_LOGGER_COLOR_PREFIX "[01;33m"

    #define CC_LOGS_LOGGER_COLOR(a_name)      CC_LOGS_LOGGER_ ## a_name ## _COLOR

#else
    
    #define CC_LOGS_LOGGER_RED_COLOR
    #define CC_LOGS_LOGGER_GREEN_COLOR

    #define CC_LOGS_LOGGER_CYAN_COLOR
    #define CC_LOGS_LOGGER_LIGHT_CYAN_COLOR

    #define CC_LOGS_LOGGER_BLUE_COLOR
    #define CC_LOGS_LOGGER_LIGHT_BLUE_COLOR

    #define CC_LOGS_LOGGER_LIGHT_GRAY_COLOR
    #define CC_LOGS_LOGGER_DARK_GRAY_COLOR

    #define CC_LOGS_LOGGER_WHITE_COLOR
    #define CC_LOGS_LOGGER_YELLOW_COLOR
    #define CC_LOGS_LOGGER_ORANGE_COLOR

    #define CC_LOGS_LOGGER_WARNING_COLOR
    #define CC_LOGS_LOGGER_RESET_COLOR

    #define CC_LOGS_LOGGER_COLOR(a_name)

#endif

namespace cc
{

    namespace logs
    {
    
        class Logger
        {
            
        public: // Data Type(s)
            
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
            
        protected: // Data Type(s)
            
            /**
             * An object that defines a token.
             */
            class Token
            {
                
            public: // Const Data
                
                const std::string name_;
                const std::string uri_;
                
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
                    : name_(a_name), uri_(a_fn), fp_(a_fp)
                {
                    /* empty */
                }
                
                /**
                 * @brief Destructor.
                 */
                virtual ~Token ()
                {
                    if ( nullptr != fp_ ) {
                        // ... flush it ...
                        fflush(fp_);
                        // ... and close it ...
                        if ( stderr != fp_ && stdout != fp_ ) {
                            fclose(fp_);
                        }
                    }
                }
                
            };
            
        protected: // Threading
            
            std::mutex mutex_;

        protected: // Data

            uid_t user_id_;
            gid_t group_id_;
            
        protected: // Data
            
            char*  buffer_;
            size_t buffer_capacity_;
                        
        protected: // Data
            
            std::map<std::string, Token*> tokens_;
            
        public: // Constructor(s) / Destructor
            
            Logger ()
            {
                user_id_         = UINT32_MAX;
                group_id_        = UINT32_MAX;
                buffer_          = nullptr;
                buffer_capacity_ = 0;
            }
            
            virtual ~Logger ()
            {
                if ( nullptr != buffer_ ) {
                    delete [] buffer_;
                }
            }

        public: // Initialization / Release API - Method(s) / Function(s)
            
            void    Startup  ();
            void    Shutdown ();
            
        public: // Method(s) / Function(s)

            void Register     (const std::string& a_token, const std::string& a_file);
            bool IsRegistered (const std::string& a_token);
            
            void Unregister   (const std::string& a_token);
                        
            bool EnsureOwnership (uid_t a_user_id, gid_t a_group_id);
            void Recycle         ();

        protected: // Method(s) / Function(s)
            
            bool EnsureOwnership ();
            bool EnsureBufferCapacity (const size_t& a_capacity);

        }; // end of class 'Logger'
        
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
         * @param a_token
         * @param a_file
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
                throw RegistrationException(
                    "An error occurred while creating preparing log file '" + a_file + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !"
                );
            }
            // ... keep track of it ...
            tokens_[a_token] = new Token(a_token, a_file, fp);
        }
        
        /**
         * @brief Check if a token is registered.
         *
         * @param a_token
         *
         * @return True if so, false otherwise.
         */
        inline bool Logger::IsRegistered (const std::string& a_token)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // ... exists?
            return ( tokens_.end() != tokens_.find(a_token) );
        }
    
        /**
         * @brief Call this method to unregister a token.
         *
         * @param a_token
         */
        inline void Logger::Unregister (const std::string& a_token)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // ... registered? ...
            const auto it = tokens_.find(a_token);
            if ( tokens_.end() == it ) {
                // ... no ...
                return;
            }
            // ... still open?
            if ( nullptr != it->second->fp_ ) {
                // ... flush it ...
                fflush(it->second->fp_);
                // ... let close be done by delete ...
            }
            // ... delete it ..
            delete it->second;
            // ... forget it ...
            tokens_.erase(it);
        }
    
        /**
         * @brief Change the logs permissions to a specific user / group.
         *
         * @param a_user_id
         * @param a_group_id
         *
         * @return True if all files changed to new permissions or it not needed, false otherwise.
         */
        inline bool Logger::EnsureOwnership (uid_t a_user_id, gid_t a_group_id)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            user_id_  = a_user_id;
            group_id_ = a_group_id;
            return EnsureOwnership();
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
                it.second->fp_ = fopen(it.second->uri_.c_str(), "w");
                if ( nullptr == it.second->fp_ ) {
                    const char* const err_str = strerror(errno);
                    throw RegistrationException(
                        "An error occurred while creating rotating log file '" + it.second->uri_ + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !"
                    );
                }
                // ... write a comment line ...
                fprintf(it.second->fp_, "---- %s '%s' ----\n", cc::UTCTime::NowISO8601WithTZ().c_str(), it.second->uri_.c_str());
                fflush(it.second->fp_);
            }
            EnsureOwnership();
        }
    
        /**
         * @brief Change the logs permissions to a specific user / group.
         *
         * @return True if all files changed to new permissions or it not needed, false otherwise.
         */
        inline bool Logger::EnsureOwnership ()
        {
            if ( UINT32_MAX == user_id_ || UINT32_MAX == group_id_ ) {
                return true;
            }
            size_t count = 0;
            for ( auto it : tokens_ ) {
                const int chown_status = chown(it.second->uri_.c_str(), user_id_, group_id_);
                const int chmod_status = chmod(it.second->uri_.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                if ( 0 == chown_status && 0 == chmod_status ) {
                    count++;
                }
            }
            return ( tokens_.size() == count );
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
           return ( a_capacity == buffer_capacity_ );
       }

    } // end of namespace 'logs'

} // end of namespace 'cc'

#endif // NRS_CC_LOGS_LOGGER_H_
