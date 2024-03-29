/**
 * @file jsonapi.h - PostgreSQL
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
#ifndef NRS_EV_POSTGRESQL_JSON_API_H_
#define NRS_EV_POSTGRESQL_JSON_API_H_

#include "ev/loggable.h"

#include "ev/scheduler/scheduler.h"

#include <string>        // std::string

namespace ev
{
    
    namespace postgresql
    {
        
        /**
         * @brief A class that defined a JSON API inteface.
         */
        class JSONAPI final : public ::ev::scheduler::Scheduler::Client
        {
            
        public: // Data Types
            
            /**
             * A collection of URIs to load document data.
             */
            class URIs
            {
                
            protected: // Data
                
                std::string base_;   //!< Base URL.
                std::string load_;   //!< Load URI.
                std::string params_; //!< Load URI params.
                
            public: // Constructor(s) / Destructor
                
                virtual ~URIs ()
                {
                    /* empty */
                }
                
            public: // Method(s) / Function(s)
                
                /**
                 * @brief Set base URI.
                 */
                inline void SetBase (const std::string& a_uri)
                {
                    base_ = a_uri;
                }
                
                /**
                 * @return Base URI.
                 */
                inline const std::string& GetBase () const
                {
                    return base_;
                }
                
                /**
                 * @brief Set load URI.
                 *
                 * @param a_uri
                 * @param a_a
                 */
                inline void SetLoad (const std::string& a_uri, const std::string& a_params)
                {
                    load_   = a_uri;
                    params_ = a_params;
                }
                
                /**
                 * @return Load URI.
                 */
                inline const std::string& GetLoad () const
                {
                    return load_;
                }
                
                /**
                 * @return Load URI params.
                 */
                inline const std::string& GetLoadParams () const
                {
                    return params_;
                }
                
            };
            
        public: // Data Type(s)
            
            typedef std::function<void(const char* a_uri, const char* a_json, const char* a_error, uint16_t a_status, uint64_t a_elapsed)> Callback;
            
        private: // Const Refs
            
            const Loggable::Data& loggable_data_ref_;
            
        private: // data
            
            URIs         uris_;                 //!< The required URIs.
            std::string  user_id_;              //!< Current user id.
            std::string  company_id_;           //!< Current company id.
            std::string  company_schema_;       //!< Current company schema.
            std::string  sharded_schema_;       //!< Current sharded schema.
            std::string  accounting_schema_;    //!< Current accounting schema.
            std::string  accounting_prefix_;    //!< Current accounting table prefix. 
            
        public: // Constructor(s) / Destructor
            
            JSONAPI (const Loggable::Data& a_loggable_data_ref);
            virtual ~JSONAPI ();
            
        public: // API Method(s) / Function(s) - declaration
            
            virtual void Get    (const std::string& a_uri,                            Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Get    (const Loggable::Data& a_loggable_data,
                                 const std::string& a_uri,                            Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Post   (const std::string& a_uri, const std::string& a_body, Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Post   (const Loggable::Data& a_loggable_data,
                                 const std::string& a_uri, const std::string& a_body, Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Patch  (const std::string& a_uri, const std::string& a_body, Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Patch  (const Loggable::Data& a_loggable_data,
                                 const std::string& a_uri, const std::string& a_body, Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Delete (const std::string& a_uri, const std::string& a_body, Callback a_callback,
                                 std::string* o_query = nullptr);
            virtual void Delete (const Loggable::Data& a_loggable_data,
                                 const std::string& a_uri, const std::string& a_body, Callback a_callback,
                                 std::string* o_query = nullptr);

        public: // API Configuration - Method(s) / Function(s) - declaration
            
            URIs&              GetURIs              ();
            
            void               SetUserId            (const std::string& a_id);
            const std::string& GetUserId            () const;

            void               SetCompanyId         (const std::string& a_id);
            const std::string& GetCompanyId         () const;

            void               SetCompanySchema     (const std::string& a_schema);
            const std::string& GetCompanySchema     () const;

            void               SetShardedSchema     (const std::string& a_schema);
            const std::string& GetShardedSchema     () const;
            
            void               SetAccountingSchema  (const std::string& a_schema);
            const std::string& GetAccountingSchema  () const;
            
            void               SetAccountingPrefix  (const std::string& a_prefix);
            const std::string& GetAccountingPrefix  () const;


        protected:
            
            void                   AsyncQuery (const ::ev::Loggable::Data& a_loggable_data,
                                               const std::string a_uri, Callback a_callback,
                                               std::string* o_query = nullptr);
            ::ev::scheduler::Task* NewTask    (const EV_TASK_PARAMS& a_callback);
            
        public: // STATIC API METHOD(S) / FUNCTION(S)

            static void SQLEscape (const std::string& a_value, std::string& o_value);

        }; // end of class JSONAPI
        
        /**
         * @return Access to JSON API URI's.
         */
        inline JSONAPI::URIs& JSONAPI::GetURIs ()
        {
            return uris_;
        }
        
        /**
         * @brief Set the user ID
         *
         * @param a_id the user ID
         */
        inline void JSONAPI::SetUserId (const std::string& a_id)
        {
            user_id_ = a_id;
        }
        
        /**
         * @return User ID
         */
        inline const std::string& JSONAPI::GetUserId () const
        {
            return user_id_;
        }        
        /**
         * @brief Set the company ID
         *
         * @param a_id the company ID
         */
        inline void JSONAPI::SetCompanyId (const std::string& a_id)
        {
            company_id_ = a_id;
        }

        /**
         * @return Company ID
         */
        inline const std::string& JSONAPI::GetCompanyId () const
        {
            return company_id_;
        }
        
        /**
         * @brief Set DB company schema name.
         *
         * @param a_schema The company specific schema name
         */
        inline void JSONAPI::SetCompanySchema (const std::string& a_schema)
        {
            company_schema_ = a_schema;
        }
        
        /**
         * @return Company schema name.
         */
        inline const std::string& JSONAPI::GetCompanySchema () const
        {
            return company_schema_;
        }
        
        /**
         * @brief Set DB sharded schema name.
         *
         * @param a_schema The DB schema for partially sharded companies (public for unsharded, same as company_schema for sharded)
         */
        inline void JSONAPI::SetShardedSchema (const std::string& a_schema)
        {
            sharded_schema_ = a_schema;
        }
        
        /**
         * @return DB sharded schema name.
         */
        inline const std::string& JSONAPI::GetShardedSchema() const
        {
            return sharded_schema_;
        }

        /**
         * @brief Set the DB accounting schema 
         *
         * @param The sharding schema for accounting tables and functions
         */
        inline void  JSONAPI::SetAccountingSchema (const std::string& a_schema)
        {
            accounting_schema_ = a_schema;
        }

        inline const std::string& JSONAPI::GetAccountingSchema () const
        {
            return accounting_schema_;
        }

        /**
         * @brief Set DB table prefix.
         *
         * @param a_prefix The accounting prefix for accounting functions
         */
        inline void JSONAPI::SetAccountingPrefix (const std::string& a_prefix)
        {
            accounting_prefix_ = a_prefix;
        }
        
        /**
         * @return DB schema name.
         */
        inline const std::string& JSONAPI::GetAccountingPrefix () const
        {
            return accounting_prefix_;
        }

    } // end of namespace postgres
    
} // end of namespace ev

#endif // NRS_EV_POSTGRESQL_JSON_API_H_
