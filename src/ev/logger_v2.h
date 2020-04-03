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

#include "cc/logs/logger.h"

#include "cc/utc_time.h"

#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h> // chmod
#include <unistd.h> // getpid, chown
#include <mutex>    // std::mutext, std::lock_guard
#include <string.h> // stderror
#include <string>
#include <map>
#include <set>      // std::set
#include <vector>   // std::vector
#include <exception>

#include "ev/loggable.h"

#include "osal/osal_singleton.h"

#ifndef EV_LOGGER_KEY_FMT
#define EV_LOGGER_KEY_FMT "%-28.28s"
#endif

namespace ev
{
        
    class LoggerV2 final : public ::cc::logs::Logger, public osal::Singleton<LoggerV2>
    {
        
    public: // Data Types
        
        class Client
        {
            
        public: // Const Refs
            
            const ev::Loggable::Data& loggable_data_ref_;
            
        protected: // Data
            
            std::string           prefix_;
            size_t                prefix_changes_count_;
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
                prefix_changes_count_ = 0;
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
                UpdateLoggerPrefix();
                tokens_.clear();
                for ( auto token : a_tokens ) {
                    tokens_.insert(token);
                }
            }
            
            /**
             * @brief Update this client logger prefix and enabled tokens.
             */
             inline void UpdateLoggerPrefix ()
            {
                std::string module_name;
                if ( strlen(loggable_data_ref_.module()) > 22 ) {
                    module_name = "..." + std::string(loggable_data_ref_.module() + ( strlen(loggable_data_ref_.module()) + 3 - 22));
                } else {
                    module_name = loggable_data_ref_.module();
                }
                std::string tag;
                if ( strlen(loggable_data_ref_.tag()) > 30 ) {
                    tag = "..." + std::string(loggable_data_ref_.tag() + ( strlen(loggable_data_ref_.tag()) + 3 - 30));
                } else {
                    tag = loggable_data_ref_.tag();
                }
                // ... logger ...
                char prefix [256] = { 0 };
                snprintf(prefix, sizeof(prefix) / sizeof(prefix[0]),
                         "%8u, %15.15s, %22.22s, %32.32s, %p, ",
                         static_cast<unsigned>(getpid()),
                         loggable_data_ref_.ip_addr(),
                         module_name.c_str(),
                         tag.c_str(),
                         loggable_data_ref_.owner_ptr()
                );
                prefix_ = prefix;
                if ( true == prefix_changed() ) {
                    prefix_changes_count_ += 1;
                }
            }
            
            /**
             * @return Read-only access to prefix.
             */
            inline const char* prefix () const
            {
                return prefix_.c_str();
            }
            
            /**
             * @return True if prefix has changed.
             */
            inline bool prefix_changed ()
            {
                return loggable_data_ref_.changed(prefix_changes_count_);
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

    private: // Data
        
        std::set<Client*>              clients_;
        std::map<std::string, ssize_t> counter_;
        
    public: // Registration API - Method(s) / Function(s)
        
        bool        IsRegistered (Client* a_client, const std::string& a_token);
        void        Register     (Client* a_client, const std::set<std::string>& a_tokens);
        std::string Prefix       (Client* a_client) const;
        void        Unregister   (Client* a_client);
        bool        Using        (Client* a_client, int a_fd);

        ssize_t  Count           (const char* const a_protocol);
        
        
    public: // Log API - Method(s) / Function(s)
        
        void     Log (Client* a_client,
                      const char* const a_token, const char* a_format, ...) __attribute__((format(printf, 4, 5)));
        
        void     Log (Client* a_client, const char* const a_token, const std::vector<std::string>& a_lines);

    public: // Static Method(s) / Function(s)
        
        static size_t NumberOfDigits (size_t a_value);

    }; // end of class Logger

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
     * @brief Check if a client is using a file descriptor.
     *
     * @param a_client Optional, if null check if any client is using it else check for this client only.
     * @param a_fd
     *
     * @return True if the token is in use by all or a specific client.
     */
    inline bool LoggerV2::Using (Client* a_client, int a_fd)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if ( nullptr != a_client ) {
            const auto it = clients_.find(a_client);
            if ( clients_.end() == it ) {
                return false;
            }
            for ( auto it2 : tokens_ ) {
                if ( fileno(it2.second->fp_) == a_fd ) {
                    if ( a_client->tokens().end() != a_client->tokens().find(it2.first) ) {
                        return true;
                    }
                }
            }
        } else {
            for ( auto it : tokens_ ) {
                if ( fileno(it.second->fp_) == a_fd ) {
                    return true;
                }
            }
        }

        return false;
    }
    
    /**
     * @brief Count number of registered clients for a specific 'name'.
     *
     * @param a_name
     *
     * @return The number of registered clients for a specific 'name'.
     */
    inline ssize_t LoggerV2::Count (const char* const a_name)
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
        // ...
        if ( true == a_client->prefix_changed() ) {
            a_client->UpdateLoggerPrefix();
        }
        // ... ensure we have a valid buffer ...
        if ( false == EnsureBufferCapacity(1024) ) {
            // ... oops ...
            return;
        }
        int aux = INT_MAX;
        // ... try at least 2 times to construct the output message ...
        for ( uint8_t attempt = 0 ; attempt < 2 ; ++attempt ) {
            buffer_[0] = '\0';
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
                    cc::UTCTime::NowISO8601WithTZ().c_str(), a_client->prefix()
            );
            fprintf(tokens_.find(a_token)->second->fp_, "%s\n", buffer_);
            // ... flush ...
            if ( stdout != file && stderr != file ) {
                fflush(file);
            }
        }
    }

    /**
     * @brief Output a log message if the provided token is registered.
     *
     * @param a_client
     * @param a_token
     * @param a_lines
     */
    inline void LoggerV2::Log (Client* a_client, const char* const a_token, const std::vector<std::string>& a_lines)
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
        // ...
        if ( true == a_client->prefix_changed() ) {
            a_client->UpdateLoggerPrefix();
        }
        // ...
        auto file = tokens_.find(a_token)->second->fp_;
        // ... output message ...
        for ( auto& line : a_lines ) {
            fprintf(tokens_.find(a_token)->second->fp_, "%s,%s%s\n",
                    cc::UTCTime::NowISO8601WithTZ().c_str(), a_client->prefix(), line.c_str()
            );
        }
        // ... flush ...
        if ( stdout != file && stderr != file ) {
            fflush(file);
        }
    }
    
} // end of namespace 'ev'

#endif // NRS_EV_LOGGER_V2_H_
