/**
 * @file request.h
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
#ifndef NRS_EV_CURL_REQUEST_H_
#define NRS_EV_CURL_REQUEST_H_

#include "ev/request.h"

#include "ev/curl/object.h" // EV_CURL_HEADERS_MAP

#include <string> // std::string
#include <vector> // std::vector
#include <map>    // std::map
#include <chrono>

#include <curl/curl.h>

#include "cc/fs/file.h"
#include "cc/fs/exception.h"

namespace ev
{
    namespace curl
    {

        class Request : public ev::Request
        {

        public: // Data Type(s)

            enum class HTTPRequestType : uint8_t
            {
                GET,
                PUT,
                DELETE,
                POST,
                PATCH,
                HEAD
            }; // end of enum 'HTTPRequestType'

        private: // Data Types

            enum class Step : uint8_t
            {
                NotSet,
                ReadingHeaders,
                ReadingBody,
                WritingHeaders,
                WritingBody
            }; // end of enum 'Step'

        private: // Const Data

            const HTTPRequestType              http_request_type_; //!< One of \link HTTPRequestType \link.

        private: // Data

            std::string                        url_;                  //!< Request URL.
            long                               connection_timeout_;   //!< Number of seconds to consider connection timeout, 0 disabled.
            long                               operation_timeout_;    //!< Number of seconds to consider operation timeout, 0 disabled.
            long                               low_speed_limit_;      //!< Low speed limit in bytes per second, 0 disabled.
            long                               low_speed_time_;       //!< Low speed limit time period, 0 disabled.
            curl_off_t                         max_recv_speed_;       //!< Rate limit data download speed, 0 disabled.
            int                                initialization_error_; //!< Error codes accumulator.
            bool                               aborted_;              //!< When true the current request must be aborder a.s.a.p.
            struct curl_slist*                 headers_;              //!< Request out headers.
            CURL*                              handle_;               //!< CURL handle.

            Step                               step_;                 //!< One of \link Step \link.
            EV_CURL_HEADERS_MAP                rx_headers_;           //!< Received headers.
            std::string                        rx_body_;              //!<
            std::string                        tx_body_;              //!<
            size_t                             tx_count_;             //!<
            
            ::cc::fs::file::Writer             rx_fw_;                //!< Response file writer.
            std::string                        rx_uri_;               //!< FILE URI where the received response will be written to.
            ::cc::fs::Exception*               rx_exp_;               //!< Set if an exception ocurred while writing data to file.

            ::cc::fs::file::Reader             tx_fr_;                //!< Request file reader.
            std::string                        tx_uri_;               //!< FILE URI where the sending body will be read from.
            ::cc::fs::Exception*               tx_exp_;               //!< Set if an exception ocurred while reading data from file.
            
            std::chrono::steady_clock::time_point s_tp_;
            std::chrono::steady_clock::time_point e_tp_;

        private: // Const Data

            std::string                        dummy_;                //!<

        public: // Constructor(s) / Destructor

            Request (const Loggable::Data& a_loggable_data,
                     const HTTPRequestType& a_type, const std::string& a_url,
                     const EV_CURL_HEADERS_MAP* a_headers = nullptr, const std::string* a_body = nullptr);
            virtual ~Request();

        public: // Inherited Virtual Method(s) / Function(s)

            virtual const char*        AsCString () const;
            virtual const std::string& AsString  () const;

        public: // Inline Method(s) / Function(s)

            CURL*                      easy_handle ();
            const std::string&         url         () const;
            const EV_CURL_HEADERS_MAP& rx_headers  () const;
            
            const ::cc::fs::Exception* rx_exp      () const;
            const ::cc::fs::Exception* tx_exp      () const;
            
            void                       SetReadBodyFrom        (const std::string& a_uri);
            void                       SetWriteResponseBodyTo (const std::string& a_uri);
            void                       Close                  ();
            
            void                       SetStarted  ();
            void                       SetFinished ();
            size_t                     Elapsed     ();

        protected:

            virtual size_t OnHeaderReceived             (void* a_ptr, size_t a_size, size_t a_nm_elem);
            virtual int    OnProgressChanged            (curl_off_t a_dl_total, curl_off_t a_dl_now, curl_off_t a_ul_total, curl_off_t a_ul_now);
            virtual size_t OnBodyReceived               (char* a_buffer, size_t a_size, size_t a_nm_elem);
            virtual size_t OnSendBody                   (char* o_buffer, size_t a_size, size_t a_nm_elem);

        private: // Static Method(s) / Function(s) Declaration - (lib)cURL callback(s)

            static size_t HeaderFunctionCallbackWrapper (void* a_ptr, size_t a_size, size_t a_nm_elem, void* a_self);
            static int    ProgressCallbackWrapper       (void* a_self, curl_off_t a_dl_total, curl_off_t a_dl_now, curl_off_t a_ul_total, curl_off_t a_ul_now);

            static size_t WriteDataCallbackWrapper      (char* a_buffer, size_t a_size, size_t a_nm_elem, void* a_self);
            static size_t ReadDataCallbackWrapper       (char* o_buffer, size_t a_size, size_t a_nm_elem, void* a_self);

        }; // end of class 'Request'

        /**
         * @return Access to CURL easy handle.
         */
        inline CURL* Request::easy_handle ()
        {
            return handle_;
        }

        /**
         * @return Readonly access to URL.
         */
        inline const std::string& Request::url () const
        {
            return url_;
        }

        /**
         * @return Received headers.
         */
        inline const EV_CURL_HEADERS_MAP& Request::rx_headers() const
        {
            return rx_headers_;
        }
        
        /**
         * @return R/O access to an exception occurred while writing incomming data to a file .
         */
        inline const ::cc::fs::Exception* Request::rx_exp () const
        {
            return rx_exp_;
        }
        
        /**
         * @return R/O access to an exception occurred while writing reading data from a file .
         */
        inline const ::cc::fs::Exception* Request::tx_exp () const
        {
            return tx_exp_;
        }    

        /**
         * @brief Send body from a file.
         *
         * @param a_uri Local file uri.
         */
        inline void Request::SetReadBodyFrom (const std::string& a_uri)
        {
            tx_uri_ = a_uri;
            tx_fr_.Open(tx_uri_, ::cc::fs::file::Reader::Mode::Read);
        }

        /**
         * @brief Write response body to a file.
         *
         * @param a_uri Local file uri.
         */
        inline void Request::SetWriteResponseBodyTo (const std::string& a_uri)
        {
            rx_uri_ = a_uri;
            rx_fw_.Open(rx_uri_, ::cc::fs::file::Writer::Mode::Write);
        }
    
        /**
         * @brief Close all open files ( if any ).
         */
        inline void Request::Close ()
        {
            if ( true == rx_fw_.IsOpen() ) {
                rx_fw_.Flush();
                rx_fw_.Close();
            }
            if ( true == tx_fr_.IsOpen() ) {
                tx_fr_.Close();
            }
        }
    
        /**
         * @brief Set start time point.
         */
        inline void Request::SetStarted ()
        {
            s_tp_ = std::chrono::steady_clock::now();
        }

        /**
         * @brief Set finish time point.
         */
        inline void Request::SetFinished ()
        {
            e_tp_ = std::chrono::steady_clock::now();
        }
    
        /**
         * @brief Set finish time point.
         */
        inline size_t Request::Elapsed ()
        {
            return static_cast<size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(e_tp_ - s_tp_).count());
        }

    } // end of namespace 'curl'

} // end of namespace 'ev'

#endif // NRS_EV_CURL_REQUEST_H_
