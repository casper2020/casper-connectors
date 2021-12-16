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
 * @param a_timeouts
 */
ev::curl::Request::Request (const ::ev::Loggable::Data& a_loggable_data,
                            const ev::curl::Request::HTTPRequestType& a_type, const std::string& a_url,
                            const EV_CURL_HEADERS_MAP* a_headers, const std::string* a_body, const Request::Timeouts* a_timeouts)
    : ev::Request(a_loggable_data, ev::Object::Target::CURL, ev::Request::Mode::OneShot), http_request_type_(a_type)
{
    url_                  = a_url;
    timeouts_.connection_ = ( nullptr != a_timeouts && -1 != a_timeouts->connection_ ? std::max(1l, a_timeouts->connection_) :   30 );
    timeouts_.operation_  = ( nullptr != a_timeouts && -1 != a_timeouts->operation_  ? std::max(1l, a_timeouts->operation_)  : 3600 );
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
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT      , timeouts_.connection_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_TIMEOUT             , timeouts_.operation_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION    , ProgressCallbackWrapper);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_PROGRESSDATA        , this);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_NOPROGRESS          , 0);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_LIMIT     , low_speed_limit_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_TIME      , low_speed_time_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_MAX_RECV_SPEED_LARGE, max_recv_speed_);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION      , HeaderFunctionCallbackWrapper);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HEADERDATA          , this);
    initialization_error_ += curl_easy_setopt(handle_, CURLOPT_FORBID_REUSE        , 1L);
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
        case ev::curl::Request::HTTPRequestType::PATCH:
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PATCH");
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

    tx_body_ = nullptr != a_body ? *a_body : "";

    // ... setup handle callbacks according to HTTP request type ...
    switch ( http_request_type_ ) {
        case ev::curl::Request::HTTPRequestType::POST:
        {
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE   , (curl_off_t)tx_body_.size());
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_READDATA        , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_READFUNCTION    , ReadDataCallbackWrapper);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEDATA       , this);
            initialization_error_ += curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION   , WriteDataCallbackWrapper);
        }
            break;
        case ev::curl::Request::HTTPRequestType::PUT:
        case ev::curl::Request::HTTPRequestType::DELETE:
        case ev::curl::Request::HTTPRequestType::PATCH:
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
            const std::string header = h_it->first + ":" + ( h_it->second.front().length() > 0 ? " " + h_it->second.front() : "" );
            headers_ = curl_slist_append(headers_, header.c_str());
            if ( nullptr == headers_ ) {
                if ( nullptr != handle_ ) {
                    curl_easy_cleanup(handle_);
                    handle_ = nullptr;
                }
                throw ev::Exception("Unable to append request headers - nullptr!");
            }
            tx_headers_[h_it->first] = h_it->second;
        }
        initialization_error_ += curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers_);
    }


    if ( ev::curl::Request::HTTPRequestType::POST == http_request_type_ ) {
        const std::string content_length = "Content-Length: " + std::to_string(tx_body_.size());
        headers_ = curl_slist_append(headers_, content_length.c_str());
        if ( nullptr == headers_ ) {
            if ( nullptr != handle_ ) {
                curl_easy_cleanup(handle_);
                handle_ = nullptr;
            }
            throw ev::Exception("Unable to append request headers - nullptr!");
        }
    }
    
    step_     = ev::curl::Request::Step::NotSet;
    tx_count_ = 0;

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
        tx_body_ = "";
        throw ev::Exception("Unable to initializer CURL handle - error code %d!", initialization_error_);
    }
    
    rx_exp_ = nullptr;
    tx_exp_ = nullptr;
    
    s_tp_ = std::chrono::steady_clock::now();
    e_tp_ = s_tp_;
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
    if ( nullptr != rx_exp_ ) {
        delete rx_exp_;
    }
    if ( nullptr != tx_exp_ ) {
        delete tx_exp_;
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
    return AsString().c_str();
}

/**
 * @return This object string representation.
 */
const std::string& ev::curl::Request::AsString () const
{
    if ( ev::curl::Request::Step::ReadingBody == step_ ) {
        if ( 0 != rx_uri_.length() ) {
            return rx_uri_;
        } else {
            return rx_body_;
        }
    } else if ( ev::curl::Request::Step::WritingBody == step_ ) {
        if ( 0 != tx_uri_.length() ) {
            return tx_uri_;
        } else {
            return tx_body_;
        }
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
 * @brief This function gets called by libcurl as soon as there is data received that needs to be saved.
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
 * @brief This function gets called by libcurl as soon as there is data received that needs to be saved.
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
        if ( 0 != rx_uri_.length() ) {
            try {
                rx_fw_.Write((const unsigned char *)(a_buffer), bytes_received);
                rx_fw_.Flush();
            } catch (const ::cc::fs::Exception& a_fs_exception) {
                rx_exp_  = new ::cc::fs::Exception(a_fs_exception);
                aborted_ = true;
            }
        } else {
            rx_body_ += std::string(a_buffer, bytes_received);
        }
    }

    return ( false == aborted_ ? rv : ( rv + 1 ) );
}

/**
 * @brief This function gets called by libcurl as soon as it needs to read data in order to send it to the peer.
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

    size_t bytes_to_send;
    
    // ... read data ...
    if ( 0 != tx_uri_.length() ) {
        // ... from file ...
        try {
            // ... bytes to send is the maximum available to read and that fix the provided buffer ...
            bytes_to_send = std::min(max_bytes_to_send, std::min(static_cast<size_t>(tx_fr_.Size()) - tx_count_, max_bytes_to_send));
            if ( 0 != bytes_to_send ) {
                // ... prepare ...
                bool eof = false;
                unsigned char* buffer = reinterpret_cast<unsigned char*>(o_buffer);
                // ... read from file ...
                const size_t bytes_read = tx_fr_.Read(buffer, bytes_to_send, eof);
                // ... adjust number of bytes to send ...
                if ( bytes_read != bytes_to_send ) {
                    bytes_to_send = bytes_read;
                }
                // ... keep track of the number of bytes sent ...
                tx_count_ += bytes_to_send;
            }
        } catch (const ::cc::fs::Exception& a_fs_exception) {
            tx_exp_  = new ::cc::fs::Exception(a_fs_exception);
            aborted_ = true;
        }
    } else {
        // ... from memory ...
        bytes_to_send = std::min(max_bytes_to_send, std::min(tx_body_.length() - tx_count_, max_bytes_to_send));
        if ( 0 != bytes_to_send ) {
            memcpy(o_buffer, tx_body_.c_str() + tx_count_, bytes_to_send);
            tx_count_ += bytes_to_send;
        }
    }
    
    // ... we're done ...
    return  ( true == aborted_ ? CURL_READFUNC_ABORT : bytes_to_send );
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
 * @brief This function gets called by libcurl as soon as there is data received that needs to be saved.
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
 * @brief This function gets called by libcurl as soon as there is data received that needs to be saved.
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
 * @brief This function gets called by libcurl as soon as it needs to read data in order to send it to the peer.
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

// MARK: -

/**
 * @brief Helper to escape an URL parameter value.
 *
 * @param a_value URL parameter value.
 *
 * @return URL encoded parameter value.
 */
std::string ev::curl::Request::Escape (const std::string &a_value)
{
    std::string rv;
    CURL *curl = curl_easy_init();
    if ( nullptr == curl ) {
        throw ::ev::Exception("Unexpected cURL handle: nullptr!");
    }

    char *output = curl_easy_escape(curl, a_value.c_str(), static_cast<int>(a_value.length()));
    if ( nullptr == output ) {
        curl_easy_cleanup(curl);
        throw ::ev::Exception("Unexpected cURL easy escape: nullptr!");
    }
    rv = output;
    curl_free(output);
    curl_easy_cleanup(curl);
    return rv;
}
