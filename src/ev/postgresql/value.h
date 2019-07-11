/**
 * @file value.h - PostgreSQL Value
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
#ifndef NRS_EV_POSTGRESQL_VALUE_H_
#define NRS_EV_POSTGRESQL_VALUE_H_

#include <sys/types.h>
#include <stdint.h> // uint16_t
#include <vector>
#include <string>

#include <libpq-fe.h> // PG*
#include <string.h>   // strlen

#include "ev/object.h"
#include "ev/exception.h"

namespace ev
{
    namespace postgresql
    {
        
        class Value final : public ev::Object
        {
            
        public: // Data Type(s)
            
            enum class ContentType : uint16_t
            {
                Null,
                Table,
                Error,
                NotSet
            };
            
            typedef struct {
                const ExecStatusType status_;
                const char* const    message_;
            } Error;
            
        public: // Static Data
            
            static Value k_null_;
            
        protected: // Data
            
            ContentType              content_type_;  //!< Content type, one of \link ContentType \link.
            PGresult*                pg_result_;     //!< Collected PosgreSQL result.
            ExecStatusType           error_status_;  //!< Last error, one of \link ExecStatusType \link.
            char*                    error_message_; //!< Last error message.
            
        public: // Constructor(s) / Destructor
            
            Value   ();
            virtual ~Value ();
            
        public: // Operator(s) Overload
            
            void operator= (PGresult* a_result);
            void operator= (const Error& a_error);
            
        public: // Inline Method(s) / Function(s)
            
            const ContentType content_type  () const;
            const bool        is_null       () const;
            const bool        is_error      () const;
            const char* const error_message () const;
            ExecStatusType    status        () const;
            const int         columns_count () const;
            const int         rows_count    () const;
            const char* const raw_value     (const size_t a_row, const size_t a_column) const;

        private: // Inline  Method(s) / Function(s)
            
            void Reset (const ContentType& a_content_type);
            
        };

        /**
         * @brief Operator '='.
         */
        inline void Value::operator= (PGresult* a_result)
        {
            if ( nullptr != a_result ) {
                Reset(Value::ContentType::Table);
            } else {
                Reset(Value::ContentType::Null);
            }
            pg_result_ = a_result;
        }
        
        inline void Value::operator= (const Error& a_error)
        {
            Reset(Value::ContentType::Error);
            if ( nullptr != a_error.message_ ) {
                error_message_ = new char[strlen(a_error.message_) + sizeof(char)];
                strcpy(error_message_, a_error.message_);
            }
            error_status_ = a_error.status_;
        }
        
        /**
         * @return This object content type, one of \link Value::ContentType \link.
         */
        inline const Value::ContentType Value::content_type () const
        {
            return content_type_;
        }
        
        /**
         * @return True if this object should be considered null, false otherwise.
         */
        inline const bool Value::is_null () const
        {
            return (( Value::ContentType::Null == content_type_ ) || ( nullptr == pg_result_ ) );
        }
        
        /**
         * @return True if this object has an error set, false otherwise.
         */
        inline const bool Value::is_error () const
        {
            return ( Value::ContentType::Error == content_type_ );
        }
    
        /**
         * @return The error message, null if none.
         */
        inline const char* const Value::error_message () const
        {
            return error_message_;
        }

        /**
        * @return Execution status, one of \link ExecStatusType \link.
         */
        inline ExecStatusType Value::status () const
        {
            return ( nullptr != pg_result_ ? PQresultStatus(pg_result_) : ExecStatusType::PGRES_NONFATAL_ERROR );
        }
    
        /**
         * @return Number of columns.
         */
        inline const int Value::columns_count () const
        {
            return nullptr != pg_result_ ? PQnfields(pg_result_) : 0;
        }
        
        /**
         * @return Number of rows.
         */
        inline const int Value::rows_count () const
        {
            return nullptr != pg_result_ ? PQntuples(pg_result_) : 0;
        }

        /**
         * @return This object raw value, string representation.
         *
         * @param a_row
         * @param a_column
         */
        inline const char* const Value::raw_value (const size_t a_row, const size_t a_column) const
        {
            if ( nullptr == pg_result_ ) {
                throw ev::Exception("No data!");
            }
            
            const int n_rows   = PQntuples(pg_result_);
            const int n_columns = PQnfields(pg_result_);
            if ( n_rows <= 0 || n_columns <= 0 ) {
                throw ev::Exception("Out of bounds while accessing pg table!");
            }
            
            if ( a_row > static_cast<size_t>(n_rows) || a_column > static_cast<size_t>(n_columns) ) {
                throw ev::Exception("Out of bounds while accessing pg table!");
            }
            
            return PQgetvalue(pg_result_, static_cast<int>(a_row), static_cast<int>(a_column));
        }

        /**
         * @brief Release previous allocated memory and reset data.
         *
         * @param a_content_type
         */
        inline void Value::Reset (const Value::ContentType& a_content_type)
        {
            content_type_ = a_content_type;
            if ( nullptr != pg_result_ ) {
                PQclear(pg_result_);
                pg_result_ = nullptr;
            }
            if ( nullptr != error_message_ ) {
                delete [] error_message_;
                error_message_ = nullptr;
            }
            error_status_ = PGRES_COMMAND_OK;
        }
        
    } // end of namespace 'postgresql'
    
} // end of namespace 'ev'

#endif // NRS_EV_POSTGRESQL_VALUE_H_
