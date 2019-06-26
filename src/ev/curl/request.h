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

#include <curl/curl.h>

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

        private: // Const Data

            std::string                        dummy_;                //!<

        public: // Constructor(s) / Destructor

            Request (const Loggable::Data& a_loggable_data,
                     const HTTPRequestType& a_type, const std::string& a_url,
                     const EV_CURL_HEADERS_MAP* a_headers, const std::string* a_body);
            virtual ~Request();

        public: // Inherited Virtual Method(s) / Function(s)

            virtual const char*        AsCString () const;
            virtual const std::string& AsString  () const;

        public: // Inline Method(s) / Function(s)

            CURL*                      easy_handle ();
            const std::string&         url         () const;
            const EV_CURL_HEADERS_MAP& rx_headers  () const;

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

    } // end of namespace 'curl'

} // end of namespace 'ev'

#endif // NRS_EV_CURL_REQUEST_H_
