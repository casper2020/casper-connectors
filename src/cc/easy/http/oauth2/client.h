/**
 * @file client.h
 *
 * Copyright (c) 2011-2021 Cloudware S.A. All rights reserved.
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef NRS_CC_EASY_HTTP_OAUTH2_CLIENT_H_
#define NRS_CC_EASY_HTTP_OAUTH2_CLIENT_H_

#include "cc/easy/http/base.h"

namespace cc
{

    namespace easy
    {

        namespace http
        {
        
            namespace oauth2
            {
            
                class Client final : public ::cc::easy::http::Base, private ::ev::scheduler::Client
                {

                public: // Data Type(s)
                    
                    class NonStandardRequestInterceptor : public ::cc::NonCopyable, public ::cc::NonMovable
                    {

                    public: // Constructor // Destructor
                        
                        /**
                         * @brief Default constructor
                         */
                        NonStandardRequestInterceptor ()
                        {
                            /* empty */
                        }
                        
                        /**
                         * @brief Destructor.
                         */
                        virtual ~NonStandardRequestInterceptor ()
                        {
                            /* empty */
                        }
                        
                    public: // Virtual Method(s) / Function(s)
                        
                        /**
                         * @brief Called to give a change to non-standard OAuth2 requests to deny refresh operation for a specfic request.
                         * @param a_url     URL.
                         * @param a_headers OAuth2 HTTP standard headers.
                         *
                         * @return True if refresh should continue, false it should be aborted.
                         */
                        virtual bool OnUnauthorizedShouldRefresh (const std::string& /* a_url */, const oauth2::Client::Headers& /* a_headers */) const { return true; }
                        
                        /**
                         * @brief Called to give a change to non-standard OAuth2 requests ( used when server does no follow RFC or needs additional headers / data )
                         *        to set or modify non-standard data.
                         * @param a_headers OAuth2 HTTP standard headers.
                         * @param a_body    OAuth2 HTTP standard body.
                         */
                        virtual void OnOAuth2RequestSet (oauth2::Client::Headers& /* o_headers */, std::string& /* a_body */) const {}
                        
                        /**
                         * @brief Called to give a change to non-standard OAuth2 requests ( used when server does no follow RFC or needs additional headers / data )
                         *        to read non-standard data.
                         *
                         * @param a_headers       HTTP standard headers.
                         * @param a_body          Response body.
                         * @param o_scope         Tokens scope.
                         * @param o_access_token  Value of the access token.
                         * @param o_refresh_token Value of the refresh token.
                         * @param o_expires_in    Expiration in seconds.
                         */
                        virtual void OnOAuth2RequestReturned (const oauth2::Client::Headers& /* o_headers */, const std::string& /* a_body */,
                                                              std::string& /* o_scope */, std::string& /* o_access_token */, std::string& /* o_refresh_token */, size_t& /* o_expires_in */) const {}

                        /**
                         * @brief Called to give a change to non-standard OAuth2 requests ( used when server does no follow RFC or needs additional headers )
                         *        to set or modify non-standard headers.
                         * @param a_headers OAuth2 HTTP standard headers.
                         */
                        virtual void OnHTTPRequestHeaderSet (oauth2::Client::Headers& /* o_headers */) const {};
                        
                        
                        /**
                         * @brief Called to give a process to non-standard OAuth2 requests reply ( used when server does no follow RFC or needs additional headers / data )
                         *        to check if we should consider reply a "401 - Unauthorized" response.
                         * @param a_code    HTTP status code.
                         * @param a_headers OAuth2 HTTP standard headers.
                         * @param a_body    OAuth2 HTTP standard body.
                         *
                         * @return True if we should consider a 401.
                         */
                        virtual bool OnHTTPRequestReturned (const uint16_t& /* a_code */, const oauth2::Client::Headers& /* a_headers */, const std::string& /* a_body */) const { return false; };

                    };
                    
                public: // Data Type(s)
                    
                    typedef struct {
                        std::string authorization_;
                        std::string token_;
                    } URLs;
                    
                    typedef struct {
                        std::string client_id_;
                        std::string client_secret_;
                    } Credentials;
                    
                    enum class GrantType : uint8_t {
                        NotSet = 0x00,
                        AuthorizationCode,
                        ClientCredentials,
                    };
                    
                    typedef struct {
                        std::string name_;
                        GrantType   type_;
                        bool        rfc_6749_strict_;
                        bool        formpost_;
                        bool        auto_;
                    } GrantTypeConfig;
                    
                    typedef struct {
                        GrantTypeConfig grant_;
                        URLs            urls_;
                        Credentials     credentials_;
                        std::string     redirect_uri_;
                        std::string     scope_;
                    } OAuth2;
                    
                    typedef struct {
                        OAuth2 oauth2_;
                    } Config;
                    
                    typedef struct _Tokens {
                        std::string           type_;       //!< Type type.
                        std::string           access_;     //!< Access token value.
                        std::string           refresh_;    //!< Refresh token value.
                        size_t                expires_in_; //!< Number of seconds until access tokens expires.
                        std::string           scope_;      //!< Service scope.
                        std::function<void()> on_change_;  //!< Function to call when values changes.
                    } Tokens;

                private: // Const Refs
                    
                    const Config& config_;
                    
                    const bool rfc_6749_;
                    const bool formpost_;

                private: // Refs
                    
                    Tokens& tokens_;

                private: // Callbacks
                    
                    const NonStandardRequestInterceptor* nsi_ptr_; //!< When set it will be 'awesome'...
                    
                public: // Constructor / Destructor

                    Client () = delete;
                    Client (const ev::Loggable::Data& a_loggable_data, const Config& a_config, Tokens& a_tokens, const char* const a_user_agent = nullptr,
                            const bool a_rfc_6749 = true, const bool a_formpost = false);
                    virtual ~Client();
                    
                public: // Method(s) / Function(s)
                    
                    void AuthorizationCodeGrant (const std::string& a_code,
                                                 Callbacks a_callbacks);

                    void AuthorizationCodeGrant (const std::string& a_code, const std::string& a_scope, const std::string& a_state,
                                                 Callbacks a_callbacks);

                    void AuthorizationCodeGrant (Callbacks a_callbacks);

                    void ClientCredentialsGrant (Callbacks a_callbacks);
                                        
                protected: // Inherited Virtual Method(s) / Function(s) - Implementation // Overload

                    virtual void Async (const Method a_method,
                                        const std::string& a_url, const Headers& a_headers, const std::string* a_body,
                                        Callbacks a_callbacks, const Timeouts* a_timeouts);

                public: // Inline Method(s) / Function(s)

                    /**
                     * Set non-standard request interceptor.
                     *
                     * @param a_interceptor See \link NonStandardRequestInterceptor \link.
                     */
                    inline void SetNonStandardRequestInterceptor (const NonStandardRequestInterceptor* a_interceptor)
                    {
                        nsi_ptr_ = a_interceptor;
                    }

                }; // end of class 'Client'
            
            } // end of namespace 'oauth2'
        
        } // end of namespace 'http'

    }  // end of namespace 'easy'

} // end of namespace 'cc'

#endif // NRS_CC_EASY_HTTP_OAUTH2_CLIENT_H_
