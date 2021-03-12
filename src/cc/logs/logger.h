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
#include <pwd.h>      // getpwuid
#include <grp.h>      // getgrgid
#include <mutex>      // std::mutext, std::lock_guard
#include <string.h>   // stderror
#include <map>        // std::map

#include "cc/utc_time.h"

#include "cc/singleton.h"

#if defined(__APPLE__) || defined(__linux__)

    #ifdef __APPLE__
        #define CC_LOGS_LOGGER_COLOR_PREFIX "\033"
    #else // linux
        #define CC_LOGS_LOGGER_COLOR_PREFIX "\e"
    #endif

    #define CC_LOGS_LOGGER_RESET_ATTRS        CC_LOGS_LOGGER_COLOR_PREFIX "[0m"

    #define CC_LOGS_LOGGER_MAGENTA_COLOR      CC_LOGS_LOGGER_COLOR_PREFIX "[00;35m"

    #define CC_LOGS_LOGGER_RED_COLOR          CC_LOGS_LOGGER_COLOR_PREFIX "[00;31m"
    #define CC_LOGS_LOGGER_LIGHT_RED_COLOR    CC_LOGS_LOGGER_COLOR_PREFIX "[00;91m"

    #define CC_LOGS_LOGGER_GREEN_COLOR        CC_LOGS_LOGGER_COLOR_PREFIX "[00;32m"
    #define CC_LOGS_LOGGER_LIGHT_GREEN_COLOR  CC_LOGS_LOGGER_COLOR_PREFIX "[00;92m"
    
    #define CC_LOGS_LOGGER_CYAN_COLOR         CC_LOGS_LOGGER_COLOR_PREFIX "[00;36m"
    #define CC_LOGS_LOGGER_LIGHT_CYAN_COLOR   CC_LOGS_LOGGER_COLOR_PREFIX "[00;96m"

    #define CC_LOGS_LOGGER_BLUE_COLOR         CC_LOGS_LOGGER_COLOR_PREFIX "[00;34m"
    #define CC_LOGS_LOGGER_LIGHT_BLUE_COLOR   CC_LOGS_LOGGER_COLOR_PREFIX "[00;94m"
    
    #define CC_LOGS_LOGGER_LIGHT_GRAY_COLOR   CC_LOGS_LOGGER_COLOR_PREFIX "[00;37m"
    #define CC_LOGS_LOGGER_DARK_GRAY_COLOR    CC_LOGS_LOGGER_COLOR_PREFIX "[00;90m"
    
    #define CC_LOGS_LOGGER_WHITE_COLOR        CC_LOGS_LOGGER_COLOR_PREFIX "[00;97m"
    #define CC_LOGS_LOGGER_YELLOW_COLOR       CC_LOGS_LOGGER_COLOR_PREFIX "[00;33m"
    #define CC_LOGS_LOGGER_ORANGE_COLOR       CC_LOGS_LOGGER_COLOR_PREFIX "[00;33m"
    
    #define CC_LOGS_LOGGER_WARNING_COLOR      CC_LOGS_LOGGER_COLOR_PREFIX "[00;33m"

    #define CC_LOGS_LOGGER_COLOR(a_name)      CC_LOGS_LOGGER_ ## a_name ## _COLOR

#else

    #define CC_LOGS_LOGGER_MAGENTA_COLOR
    
    #define CC_LOGS_LOGGER_RED_COLOR
    #define CC_LOGS_LOGGER_LIGHT_RED_COLOR

    #define CC_LOGS_LOGGER_GREEN_COLOR
    #define CC_LOGS_LOGGER_LIGHT_GREEN_COLOR

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
    
        // ---- //
        class OneShotInitializer;

        class Logger
        {
            
            friend class OneShotInitializer;
            
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
                 * @param a_name Token name.
                 * @param a_uri  File URI.
                 * @param a_fp   FILE pointer.
                 */
                Token (const std::string& a_name, const std::string& a_uri, FILE* a_fp)
                    : name_(a_name), uri_(a_uri), fp_(a_fp)
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

            uid_t       user_id_;
            std::string user_name_;
            gid_t       group_id_;
            std::string group_name_;
            mode_t      mode_;

        protected: // Data
            
            char*  buffer_;
            size_t buffer_capacity_;
                        
        protected: // Data
            
            std::map<std::string, Token*> tokens_;

        public: // Initialization / Release API - Method(s) / Function(s)
            
            void Startup  ();
            void Shutdown ();
            
        public: // Method(s) / Function(s)

            void Register     (const std::string& a_token, const std::string& a_uri);
            bool IsRegistered (const std::string& a_token);
            
            void Unregister   (const std::string& a_token);
                        
            bool EnsureOwnership (uid_t a_user_id, gid_t a_group_id);
            void Recycle         ();

        protected: // Method(s) / Function(s)
            
            bool EnsureOwnership      ();
            bool EnsureOwnership      (const std::string& a_uri);
            bool EnsureBufferCapacity (const size_t& a_capacity);

        }; // end of class 'Logger'
        
        /**
         * @brief Initialize logger instance.
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
         * @param a_token Token name.
         * @param a_uri  File URI.
         */
        inline void Logger::Register (const std::string& a_token, const std::string& a_uri)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // ... already registered? ...
            if ( tokens_.end() != tokens_.find(a_token) ) {
                return;
            }
            // ... try to open log file ...
            FILE* fp = fopen(a_uri.c_str(), "a");
            if ( nullptr == fp ) {
                const char* const err_str = strerror(errno);
                throw RegistrationException(
                    "An error occurred while creating preparing log file '" + a_uri + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !"
                );
            }
            // ... keep track of it ...
            tokens_[a_token] = new Token(a_token, a_uri, fp);
            // ... ensure ownership ...
            EnsureOwnership(a_uri);
        }
        
        /**
         * @brief Check if a token is registered.
         *
         * @param a_token Token name.
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
         * @param a_token Token name.
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
                if ( stdout != it->second->fp_ && stderr != it->second->fp_ ) {
                    fflush(it->second->fp_);
                }
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
         * @param a_user_id  Log file owner ID.
         * @param a_group_id Log file owner group ID.
         *
         * @return True if all files changed to new permissions or it not needed, false otherwise.
         */
        inline bool Logger::EnsureOwnership (uid_t a_user_id, gid_t a_group_id)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            user_id_    = a_user_id;
            user_name_  = "";
            group_id_   = a_group_id;
            group_name_ = "";
            if ( not ( UINT32_MAX == user_id_ || 0 == user_id_) ) {
                struct passwd* pw = getpwuid(user_id_);
                if ( nullptr != pw ) {
                    user_name_ = pw->pw_name;
                }
            }
            if ( not ( UINT32_MAX == user_id_ || 0 == user_id_) ) {
                struct group* gr = getgrgid(group_id_);
                if ( nullptr != gr ) {
                    group_name_ = gr->gr_name;
                }
            }
            return EnsureOwnership();
        }
        
        /**
         * @brief Change the logs permissions to a specific user / group for a specific file.
         *
         * @param a_uri File URI.
         *
         * @return True if changed to new permissions or it not needed, false otherwise.
         */
        inline bool Logger::EnsureOwnership (const std::string& a_uri)
        {
            if ( ( UINT32_MAX == user_id_ || UINT32_MAX == group_id_ ) || ( 0 == user_id_ || 0 == group_id_ ) ) {
                return true;
            }
            const int chown_status = chown(a_uri.c_str(), user_id_, group_id_);
            if ( 0 != chown_status ) {
                fprintf(stderr, "WARNING: failed to change ownership of %s to %u:%u ~ %d - %s\n", a_uri.c_str(), user_id_, group_id_, errno, strerror(errno));
                fflush(stderr);
            }
            const int chmod_status = chmod(a_uri.c_str(), mode_);
            if ( 0 != chmod_status ) {
                fprintf(stderr, "WARNING: failed to change permissions of %s to %o ~ %d - %s\n", a_uri.c_str(), mode_, errno, strerror(errno));
                fflush(stderr);
            }
            return ( 0 == chown_status && 0 == chmod_status );
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
            }
            // ... write a comment line ...
            for ( auto it : tokens_ ) {
                // ... skip if not a 'real' file ...
                if ( stdout == it.second->fp_ || stderr == it.second->fp_ ) {
                    continue;
                }
                // .. ensure write permissions ...
                (void)EnsureOwnership(it.second->uri_);
                // ... log recycle messages ...
                fprintf(it.second->fp_, "--- --- ---\n");
                fprintf(it.second->fp_, "⌥ LOG FILE   : %s\n", it.second->uri_.c_str());
                fprintf(it.second->fp_, "⌥ OWNERSHIP  : %4o\n", mode_);
                if ( not ( ( UINT32_MAX == user_id_ || UINT32_MAX == group_id_ ) || ( 0 == user_id_ || 0 == group_id_ ) ) ) {
                    fprintf(it.second->fp_, "  - USER : " UINT32_FMT_LP(4) " - %s\n", user_id_, user_name_.c_str());
                    fprintf(it.second->fp_, "  - GROUP: " UINT32_FMT_LP(4) " - %s\n", group_id_, group_name_.c_str());
                }
                fprintf(it.second->fp_, "⌥ PERMISSIONS:\n");
                fprintf(it.second->fp_, "  - MODE : %-4o\n", mode_);
                fprintf(it.second->fp_, "⌥ RECYCLED AT: %s\n", cc::UTCTime::NowISO8601WithTZ().c_str());
                fprintf(it.second->fp_, "--- --- ---\n");
                fflush(it.second->fp_);
            }
        }
    
        /**
         * @brief Change the logs permissions to a specific user / group.
         *
         * @return True if all files changed to new permissions or it not needed, false otherwise.
         */
        inline bool Logger::EnsureOwnership ()
        {
            if ( ( UINT32_MAX == user_id_ || UINT32_MAX == group_id_ ) || ( 0 == user_id_ || 0 == group_id_ ) ) {
                return true;
            }
            size_t count = 0;
            for ( auto it : tokens_ ) {
                if ( true == EnsureOwnership(it.second->uri_.c_str()) ) {
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

        // ---- //
        class OneShotInitializer final : public ::cc::Initializer<Logger>
        {
            
        public: // Constructor(s) / Destructor
            
            /**
             * @brief Default constructor.
             *
             * @param a_instance Instance to initialize.
             */
            OneShotInitializer (Logger& a_instance)
                : ::cc::Initializer<Logger>(a_instance)
            {
                instance_.user_id_         = UINT32_MAX;
                instance_.user_name_       = "";
                instance_.group_id_        = UINT32_MAX;
                instance_.group_name_      = "";
                instance_.mode_            = ( S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
                instance_.buffer_          = nullptr;
                instance_.buffer_capacity_ = 0;
            }
            
            /**
             * @brief Destructor.
             */
            virtual ~OneShotInitializer ()
            {
                if ( nullptr != instance_.buffer_ ) {
                    delete [] instance_.buffer_;
                }
            }
            
        }; // end of class 'OneShotInitializer'

    } // end of namespace 'logs'

} // end of namespace 'cc'

#endif // NRS_CC_LOGS_LOGGER_H_
