/**
 * @file logger_v2.h
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
#ifndef NRS_EV_LOGGER_V2_H_
#define NRS_EV_LOGGER_V2_H_

#include "cc/utc_time.h"

#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h> // getpid
#include <mutex>    // std::mutext, std::lock_guard
#include <string.h> // stderror
#include <string>
#include <map>
#include <set>      // std::set
#include <exception>

#include "ev/loggable.h"

#include "osal/osal_singleton.h"

#ifndef EV_LOGGER_KEY_FMT
#define EV_LOGGER_KEY_FMT "%-28.28s"
#endif

namespace ev
{
        
    class LoggerV2 final : public osal::Singleton<LoggerV2>
    {
        
    private: // Threading
        
        std::mutex mutex_;
        
    public: // Data Types
        
        class Client
        {
            
        public: // Const Refs
            
            const ev::Loggable::Data& loggable_data_ref_;
            
        protected: // Data
            
            std::string           prefix_;
            std::set<std::string> tokens_;
            
        public: // Constructor / Destructor
            
            /**
             * @brief Default constructor.
             *
             * @param a_loggable_data_ref
             */
            Client (const ev::Loggable::Data& a_loggable_data_ref)
                : loggable_data_ref_(a_loggable_data_ref)
            {
                /* empty */
            }
            
            /**
             * @brief Destructor.
             */
            virtual ~Client ()
            {
                // ... empty ...
            }
            
        public: // Method(s) / Function(s)
            
            /**
             * @brief Set this client logger prefix and enabled tokens.
             *
             * @param a_tokens Enabled tokens.
             */
            inline void SetLoggerPrefix (const std::set<std::string>& a_tokens)
            {
                std::string module_name;
                if ( strlen(loggable_data_ref_.module()) > 22 ) {
                    module_name = "..." + std::string(loggable_data_ref_.module() + ( strlen(loggable_data_ref_.module()) + 3 - 22));
                } else {
                    module_name = loggable_data_ref_.module();
                }
                // ... logger ...
                char prefix [256] = { 0 };
                snprintf(prefix, sizeof(prefix) / sizeof(prefix[0]),
                         "%8u, %15.15s, %22.22s, %32.32s, %p, ",
                         static_cast<unsigned>(getpid()),
                         loggable_data_ref_.ip_addr(),
                         module_name.c_str(),
                         loggable_data_ref_.tag(),
                         loggable_data_ref_.owner_ptr()
                );
                prefix_ = prefix;
                tokens_ = a_tokens;
            }
            
            /**
             * @return Read-only access to prefix.
             */
            inline const char* const prefix () const
            {
                return prefix_.c_str();
            }
            
            /**
             * @return Read-only access to this client tokens.
             */
            inline const std::set<std::string>& tokens () const
            {
                return tokens_;
            }         
            
            /**
             * @brief Check if a token is already registered.
             *
             * @param a_token The token name.
             *
             * @return True when the token is already registered.
             */
            inline bool IsTokenRegistered (const std::string& a_token) const
            {
                return tokens_.end() != tokens_.find(a_token);
            }
            
        }; // end of class 'Client'
        
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
        
        std::set<Client*>              clients_;
        std::map<std::string, ssize_t> counter_;
        std::map<std::string, Token*>  tokens_;
        static char*                   s_buffer_;
        static size_t                  s_buffer_capacity_;
        
    public: // Initialization / Release API - Method(s) / Function(s)
        
        void    Startup  ();
        void    Shutdown ();
        
    public: // Registration API - Method(s) / Function(s)
        
        void     Register     (const std::string& a_token, const std::string& a_file);
        
        bool     IsRegistered (Client* a_client, const std::string& a_token);
        void     Register     (Client* a_client, const std::set<std::string>& a_tokens);
        void     Unregister   (Client* a_client);
        size_t   Count        (const char* const a_protocol);
        
    public: // Log API - Method(s) / Function(s)
        
        void     Log (Client* a_client,
                      const char* const a_token, const char* a_format, ...) __attribute__((format(printf, 4, 5)));
        
    public: // Other - Method(s) / Function(s)
        
        void     Recycle();
        
    protected: // Method(s) / Function(s)
        
        bool EnsureBufferCapacity (const size_t& a_capacity);

    }; // end of class Logger
    
    /**
     * @brief Initialize logger instance.
     */
    inline void LoggerV2::Startup  ()
    {
        std::lock_guard<std::mutex> lock(mutex_);
	if ( nullptr != s_buffer_ ) {
            delete [] s_buffer_;
        }
	s_buffer_          = new char[1024];
        s_buffer_capacity_ = nullptr != s_buffer_ ? 1024 : 0;
    }
    
    /**
     * @brief Release all dynamically allocated memory and close files ( if any )..
     */
    inline void LoggerV2::Shutdown ()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for ( auto it : tokens_ ) {
            delete it.second;
        }
        tokens_.clear();
        if ( nullptr != s_buffer_ ) {
            delete [] s_buffer_;
	    s_buffer_ = nullptr;
        }
    }
    
    /**
     * @brief Register a token.
     *
     * @param a_token
     * @param a_file
     */
    inline void LoggerV2::Register (const std::string& a_token, const std::string& a_file)
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
            throw new RegistrationException("An error occurred while creating preparing log file '" + a_file + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !");
        }
        // ... keep track of it ...
        try {
            tokens_[a_token] = new Token(a_token, a_file, fp);
        } catch (...) {
            // ... failure ...
            throw new RegistrationException("A 'C++ Generic Exception' occurred while registering log token '" + a_token + "'!");
        }
    }
    
    
    /**
     * @brief Check if a client has a token registered.
     *
     * @param a_client
     * @param a_token
     *
     * @return True if so, false otherwise.
     */
    inline bool LoggerV2::IsRegistered (Client* a_client, const std::string& a_token)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... exists?
        const auto it = clients_.find(a_client);
        if ( clients_.end() == it ) {
            // ... no, token isn't registered for it ...
            return false;
        }
        // ... exists?
        return (*it)->tokens().end() != (*it)->tokens().find(a_token);
    }
    
    /**
     * @brief Call this method to register a logger client.
     *
     * @param a_name
     * @param a_pid
     * @param a_ip_addr
     * @param a_client
     * @param a_tokens
     */
    inline void LoggerV2::Register (Client* a_client, const std::set<std::string>& a_tokens)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... already tracked?
        if ( clients_.end() != clients_.find(a_client) ) {
            // ... we're done ...
            return;
        }
        // ... keep track of client ...
        clients_.insert(a_client);
        // ... update counters ...
        const auto c_it = counter_.find(a_client->loggable_data_ref_.module());
        if ( counter_.end() == c_it ) {
            counter_[a_client->loggable_data_ref_.module()] = 1;
        } else {
            counter_[a_client->loggable_data_ref_.module()] = c_it->second + 1;
        }
        // ... set tokens ...
        a_client->SetLoggerPrefix(a_tokens);
        // ... no tokens?
        if ( 0 == a_tokens.size() ) {
            // ... we're done ...
            return;
        }
    }
    
    /**
     * @brief Call this method to unregister a logger client.
     *
     * @param a_client
     */
    inline void LoggerV2::Unregister (Client* a_client)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = clients_.find(a_client);
        if ( clients_.end() == it ) {
            return;
        }
        clients_.erase(it);
        // ... update counters ...
        const auto c_it = counter_.find(a_client->loggable_data_ref_.module());
        if ( counter_.end() != c_it ) {
            counter_[a_client->loggable_data_ref_.module()] = c_it->second - 1;
        }
    }
    
    
    /**
     * @brief Count number of registered clients for a specific 'name'.
     *
     * @param a_name
     *
     * @return The number of registered clients for a specific 'name'.
     */
    inline size_t LoggerV2::Count (const char* const a_name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto c_it = counter_.find(a_name);
        if ( counter_.end() == c_it ) {
            return 0;
        }
        return c_it->second;
    }
    
    /**
     * @brief Output a log message if the provided token is registered.
     *
     * @param a_client
     * @param a_token
     * @param a_format
     * @param ...
     */
    inline void LoggerV2::Log (Client* a_client, const char* const a_token, const char* a_format, ...)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // ...if token is not registered...
        if ( tokens_.end() == tokens_.find(a_token) ) {
            // ... we're done ...
            return;
        }
        // ... if client is not registered, or this token is not registered for it ...
        const auto client_it = clients_.find(a_client);
        if ( clients_.end() == client_it || false == (*client_it)->IsTokenRegistered(a_token) ) {
            // ... we're done ...
            return;
        }
        // ... ensure we have a valid buffer ...
        if ( false == EnsureBufferCapacity(1024) ) {
            // ... oops ...
            return;
        }
        int aux = INT_MAX;
        // ... try at least 2 times to construct the output message ...
        for ( uint8_t attempt = 0 ; attempt < 2 ; ++attempt ) {
            s_buffer_[0] = '\0';
            va_list args;
            va_start(args, a_format);
            aux = vsnprintf(s_buffer_, s_buffer_capacity_ - 1, a_format, args);
            va_end(args);
            if ( aux < 0 ) {
                // ... an error has occurred ...
                break;
            } else if ( aux > static_cast<int>(s_buffer_capacity_) ) {
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
        if ( aux > 0 && static_cast<size_t>(aux) < s_buffer_capacity_ ) {
            auto file = tokens_.find(a_token)->second->fp_;
            // ... output message ...
            fprintf(tokens_.find(a_token)->second->fp_, "%s,%s",
                    cc::UTCTime::NowISO8601WithTZ().c_str(), a_client->prefix()
            );
            fprintf(tokens_.find(a_token)->second->fp_, "%s\n", s_buffer_);
            // ... flush ...
            if ( stdout != file && stderr != file ) {
                fflush(file);
            }
        }
    }
    
    /**
     * @brief Recycle currently open log files.
     */
    inline void LoggerV2::Recycle ()
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
                throw new RegistrationException("An error occurred while creating rotating log file '" + it.second->fn_ + "': " + std::string(nullptr != err_str ? err_str : "nullptr") + " !");
            }
            // ... write a comment line ...
            fprintf(it.second->fp_, "---- NEW LOG '%s' ----\n", it.second->fn_.c_str());
            fflush(it.second->fp_);
        }
    }
    
    /**
     * @brief Ensure that the current buffer has at least the required capacity.
     *
     * @param a_capacity The required capacity ( in bytes ).
     *
     * @return True when the buffer has at least the required capacity.
     */
    inline bool LoggerV2::EnsureBufferCapacity (const size_t& a_capacity)
    {
        // ... buffer is allocated and has enough capacity? ...
        if ( s_buffer_capacity_ >= a_capacity ) {
            // ... good to go ...
            return true;
        }
        // ... if buffer is not allocated ...
        if ( nullptr != s_buffer_ ) {
            delete [] s_buffer_;
        }
        // ... try to allocate it now ...
        s_buffer_ = new char[a_capacity];
        if ( nullptr == s_buffer_ ) {
            // ... out of memory ...
            s_buffer_capacity_ = 0;
        } else {
            // ... we're good to go ....
            s_buffer_capacity_ = a_capacity;
        }
        // ... we're good to go if ...
        return ( a_capacity == s_buffer_capacity_ );
    }
    
} // end of namespace 'ev'

#endif // NRS_EV_LOGGER_V2_H_
