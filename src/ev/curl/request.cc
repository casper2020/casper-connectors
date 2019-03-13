/**
 * @file request.cc
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

#include "ev/curl/request.h"

#include <vector>  // std::vector

#include "osal/osalite.h"

/**
 * @brief Default constructor.
 *
 * @param a_loggable_data
 * @param a_logger_module
 * @param a_type
 * @param a_payload
 * @param a_headers
 */
ev::curl::Request::Request (const ::ev::Loggable::Data& a_loggable_data,
                            const ev::curl::Request::HTTPRequestType& a_type, const std::string& a_url,
                            const EV_CURL_HEADERS_MAP* a_headers = nullptr, const std::string* a_body = nullptr)
    : ev::Request(a_loggable_data, ev::Object::Target::CURL, ev::Request::Mode::OneShot), http_request_type_(a_type)
{
    url_                  = a_url;
    connection_timeout_   = 30;
    operation_timeout_    = 3600;
    low_speed_limit_      = 0; // 0 - disabled
    low_speed_time_       = 0; // 0 - disabled
    max_recv_speed_       = 0; // 0 - disabled
    initialization_error_ = CURLE_FAILED_INIT;
    aborted_              = false;
    headers_              = nullptr;
    handle_               = curl_easy_init();
    if ( nullptr == handle_ ) {
        throw ev::Exception("Failed to initialize a CURL handle!");
    }

    // ... setup common handle options ...
    initialization_error_  = curl_easy_setopt(handle_, CURLOPT_URL                 , url_.c_str());
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_NOSIGNAL            , 1);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT      , connection_timeout_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_TIMEOUT             , operation_timeout_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION    , ProgressCallbackWrapper);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_PROGRESSDATA        , this);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_NOPROGRESS          , 0);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_LIMIT     , low_speed_limit_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_TIME      , low_speed_time_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_MAX_RECV_SPEED_LARGE, max_recv_speed_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION      , HeaderFunctionCallbackWrapper);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HEADERDATA          , this);

    // ... setup handle options according to HTTP request type ...
    switch ( http_request_type_ ) {
        case ev::curl::Request::HTTPRequestType::GET:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HTTPGET, 1L);
            break;
        case ev::curl::Request::HTTPRequestType::PUT:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_PUT, 1L);
            break;
        case ev::curl::Request::HTTPRequestType::DELETE:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case ev::curl::Request::HTTPRequestType::POST:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_POST, 1L);
            break;
        case ev::curl::Request::HTTPRequestType::HEAD:
            // ... don't fetch the actual content, you only want headers ...
            initialization_error_  = curl_easy_setopt(handle_, CURLOPT_NOBODY, 1);
            break;
        default:
            curl_easy_cleanup(handle_);
            handle_ = nullptr;
            throw ev::Exception("Unsupported HTTP request type " UINT8_FMT,
				static_cast<uint8_t>(http_request_type_)
	    );
    }

    // ... setup handle callbacks according to HTTP request type ...
    switch ( http_request_type_ ) {
        case ev::curl::Request::HTTPRequestType::POST:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_READDATA        , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_READFUNCTION    , ReadDataCallbackWrapper);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEDATA       , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION   , WriteDataCallbackWrapper);
            break;
        case ev::curl::Request::HTTPRequestType::PUT:
        case ev::curl::Request::HTTPRequestType::DELETE:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_UPLOAD          , 1L);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_READDATA        , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_READFUNCTION    , ReadDataCallbackWrapper);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEDATA       , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION   , WriteDataCallbackWrapper);
            break;
        case ev::curl::Request::HTTPRequestType::GET:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEDATA       , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION   , WriteDataCallbackWrapper);
            break;
        default:
            break;
    }

    // ... append headers?
    if ( nullptr != a_headers && a_headers->size() > 0 ) {
        for ( auto h_it = a_headers->begin() ; a_headers->end() != h_it ; ++h_it ) {
            const std::string header = h_it->first + ": " + h_it->second.front();
            headers_ = curl_slist_append(headers_, header.c_str());
            if ( nullptr == headers_ ) {
                if ( nullptr != handle_ ) {
                    curl_easy_cleanup(handle_);
                    handle_ = nullptr;
                }
                throw ev::Exception("Unable to append request headers - nullptr!");
            }
        }
        initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers_);
    }


    if ( ev::curl::Request::HTTPRequestType::POST == http_request_type_ ) {
        const std::string content_length = "Content-Length: " + std::to_string( nullptr != a_body ? a_body->size() : 0 );
        headers_ = curl_slist_append(headers_, content_length.c_str());
        if ( nullptr == headers_ ) {
            if ( nullptr != handle_ ) {
                curl_easy_cleanup(handle_);
                handle_ = nullptr;
            }
            throw ev::Exception("Unable to append request headers - nullptr!");
        }
    }

    // ... check for error(s) ...
    if ( initialization_error_ != CURLE_OK ) {
        if ( nullptr != headers_ ) {
            curl_slist_free_all(headers_);
            headers_ = NULL;
        }
        if ( nullptr != handle_ ) {
            curl_easy_cleanup(handle_);
            handle_ = nullptr;
        }
        throw ev::Exception("Unable to initializer CURL handle - error code %d!", initialization_error_);
    }
    step_     = ev::curl::Request::Step::NotSet;
    tx_body_  = nullptr != a_body ? *a_body : "";
    tx_count_ = 0;
}

/**
 * @brief Destructor
 */
ev::curl::Request::~Request ()
{
    if ( nullptr != headers_ ) {
        curl_slist_free_all(headers_);
    }
    if ( nullptr != handle_ ) {
        curl_easy_cleanup(handle_);
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @return This object C string representation.
 */
const char* ev::curl::Request::AsCString () const
{
    if ( ev::curl::Request::Step::ReadingBody == step_ ) {
        return rx_body_.c_str();
    } else if ( ev::curl::Request::Step::WritingBody == step_ ) {
        return tx_body_.c_str();
    }
    return dummy_.c_str();
}

/**
 * @return This object string representation.
 */
const std::string& ev::curl::Request::AsString () const
{
    if ( ev::curl::Request::Step::ReadingBody == step_ ) {
        return rx_body_;
    } else if ( ev::curl::Request::Step::WritingBody == step_ ) {
        return tx_body_;
    }
    return dummy_;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief This function gets called by libcurl as soon as it has received header data.
 *        The header callback will be called once for each header and only complete header lines are passed on to the callback.
 *
 * @param a_ptr
 * @param a_size
 * @param a_nm_elem
 */
size_t ev::curl::Request::OnHeaderReceived (void* a_ptr, size_t a_size, size_t a_nm_elem)
{
    const size_t bytes_received = a_size * a_nm_elem;


    OSALITE_DEBUG_TRACE("curl",
                        "%p: %s : " SIZET_FMT " byte(s) received",
                        this, url_.c_str(), bytes_received
    );

    step_ = ev::curl::Request::Step::ReadingHeaders;

    if ( true == aborted_ ) {
        return 0; // ... abort now ...
    }

    if ( ( sizeof(char) * 2 ) != bytes_received ) {

        const char* const key_start_ptr = static_cast<const char* const>(a_ptr);

        const char* value_start_ptr = strchr(key_start_ptr, ':');
        if ( nullptr != value_start_ptr ) {

            const std::string key = std::string(key_start_ptr, ( value_start_ptr - key_start_ptr ));

            value_start_ptr += sizeof(char);
            if ( value_start_ptr[0] == ' ' ) {
                value_start_ptr += sizeof(char);
            }

            const char* const value_end_ptr   = strstr(value_start_ptr, "\r\n");
            if ( nullptr != value_end_ptr ) {
                rx_headers_[key].push_back(std::string(value_start_ptr, value_end_ptr - value_start_ptr));
            }

        }

        OSALITE_DEBUG_TRACE("curl",
                            "%p: %*.*s ",
                            this, bytes_received, bytes_received, key_start_ptr
        );

    }

    return bytes_received;
}

/**
 * @brief This callback function gets called by libcurl as soon as there is data received that needs to be saved.
 *
 * @param a_dl_total
 * @param a_dl_now
 * @param a_ul_total
 * @param a_ul_now
 */
int ev::curl::Request::OnProgressChanged (curl_off_t /* a_dl_total */, curl_off_t /* a_dl_now */, curl_off_t /* a_ul_total */, curl_off_t /* a_ul_now */)
{
    OSALITE_DEBUG_TRACE("curl",
                        "%p : %s ...",
                        this, url_.c_str()
    );

    if ( true == aborted_ ) {
        return -1; // ... abort now ..
    } else {
        return 0;  // ... continue ...
    }
}

/**
 * @brief This callback function gets called by libcurl as soon as there is data received that needs to be saved.
 *
 * @param a_buffer
 * @param a_size
 * @param a_nm_elem
 */
size_t ev::curl::Request::OnBodyReceived (char* a_buffer, size_t a_size, size_t a_nm_elem)
{
    const size_t bytes_received = a_size * a_nm_elem;
    const size_t rv             = bytes_received;

    OSALITE_DEBUG_TRACE("curl",
                        "%p : %s ~> " SIZET_FMT,
                        this, url_.c_str(), bytes_received
    );

    if ( ev::curl::Request::Step::ReadingBody != step_ ) {
        rx_body_ = "";
        step_    = ev::curl::Request::Step::ReadingBody;
    }

    if ( 0 != bytes_received ) {
        rx_body_ += std::string(a_buffer, bytes_received);
    }

    return ( false == aborted_ ? rv : ( rv + 1 ) );
}

/**
 * @brief This callback function gets called by libcurl as soon as it needs to read data in order to send it to the peer.
 *
 *        https://curl.haxx.se/libcurl/c/CURLOPT_READFUNCTION.html
 *
 * @param o_buffer
 * @param a_size
 * @param a_nm_elemn
 * @param a_self
 */
size_t ev::curl::Request::OnSendBody (char* o_buffer, size_t a_size, size_t a_nm_elem)
{
    const size_t max_bytes_to_send = a_size * a_nm_elem;

    OSALITE_DEBUG_TRACE("curl",
                        "%p : %s ~> " SIZET_FMT,
                        this, url_.c_str(), max_bytes_to_send
    );

    if ( ev::curl::Request::Step::WritingBody != step_ ) {
        tx_count_ = 0;
        step_     = ev::curl::Request::Step::WritingBody;
    }

    if ( true == aborted_ ) {
        return CURL_READFUNC_ABORT;
    }

    const size_t bytes_to_send = std::min(max_bytes_to_send, std::min(tx_body_.length() - tx_count_, max_bytes_to_send));
    if ( 0 != bytes_to_send ) {
        memcpy(o_buffer, tx_body_.c_str() + tx_count_, bytes_to_send);
        tx_count_ += bytes_to_send;
    }

    return bytes_to_send;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief This function gets called by libcurl as soon as it has received header data.
 *        The header callback will be called once for each header and only complete header lines are passed on to the callback.
 *
 *        https://curl.haxx.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
 *
 * @param a_ptr
 * @param a_size
 * @param a_nm_elm
 * @param a_self
 */
size_t ev::curl::Request::HeaderFunctionCallbackWrapper (void* a_ptr, size_t a_size, size_t a_nm_elem, void* a_self)
{
    return static_cast<ev::curl::Request*>(a_self)->OnHeaderReceived(a_ptr, a_size, a_nm_elem);
}

/**
 * @brief This callback function gets called by libcurl as soon as there is data received that needs to be saved.
 *
 *        https://curl.haxx.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html
 *
 * @param a_self
 * @param a_dl_total
 * @param a_dl_now
 * @param a_ul_total
 * @param a_ul_now
 */
int ev::curl::Request::ProgressCallbackWrapper (void* a_self, curl_off_t a_dl_total, curl_off_t a_dl_now, curl_off_t a_ul_total, curl_off_t a_ul_now)
{
    return static_cast<ev::curl::Request*>(a_self)->OnProgressChanged(a_dl_total, a_dl_now, a_ul_total, a_ul_now);
}

/**
 * @brief This callback function gets called by libcurl as soon as there is data received that needs to be saved.
 *
 *        https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 * @param a_buffer
 * @param a_size
 * @param a_nm_elem
 * @param a_self
 */
size_t ev::curl::Request::WriteDataCallbackWrapper (char* a_buffer, size_t a_size, size_t a_nm_elem, void* a_self)
{
    return static_cast<ev::curl::Request*>(a_self)->OnBodyReceived(a_buffer, a_size, a_nm_elem);
}

/**
 * @brief This callback function gets called by libcurl as soon as it needs to read data in order to send it to the peer.
 *
 *        https://curl.haxx.se/libcurl/c/CURLOPT_READFUNCTION.html
 *
 * @param o_buffer
 * @param a_size
 * @param a_nm_elemn
 * @param a_self
 */
size_t ev::curl::Request::ReadDataCallbackWrapper (char* o_buffer, size_t a_size, size_t a_nm_elem, void* a_self)
{
    return static_cast<ev::curl::Request*>(a_self)->OnSendBody(o_buffer, a_size, a_nm_elem);
}
