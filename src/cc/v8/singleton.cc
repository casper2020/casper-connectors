/**
 * @file singleton.cc
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
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cc/v8/singleton.h"

#include "v8-version.h"

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 */
cc::v8::Initializer::Initializer (cc::v8::Singleton& a_instance)
    : cc::Initializer<Singleton>(a_instance)
{
    instance_.initialized_ = false;
    instance_.platform_    = nullptr;
    instance_.isolate_     = nullptr;
}

/**
 * @brief Destructor.
 */
cc::v8::Initializer::~Initializer ()
{
    // ... not released yet?
    if ( true == instance_.initialized_ ) {
        // ... release isolate ...
        if ( nullptr != instance_.isolate_ ) {
            instance_.isolate_->Dispose();
        }
        ::v8::V8::Dispose();
        // platform_ will be deleted by call to
        if ( NULL != instance_.platform_ ) {
    #if V8_MAJOR_VERSION >= 10
            ::v8::V8::DisposePlatform();
    #else
            ::v8::V8::ShutdownPlatform();
    #endif
        }
        if ( nullptr != instance_.create_params_.array_buffer_allocator ) {
            delete instance_.create_params_.array_buffer_allocator;
        }
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief This method must ( and can only ) be called once to initialize V8 engine.
 */
void cc::v8::Singleton::Startup ()
{
    // ... if already initialized ...
    if ( true == initialized_ ) {
        // ... can't be initialized twice ...
        throw std::runtime_error("v8 singleton already initialized!");
    }
    // ... try to initialize ICU ...
    if ( false == ::v8::V8::InitializeICU() ) {
        throw std::runtime_error("v8 ICU initialization failure!");
    }
    // ... create new platform ..
    platform_ = ::v8::platform::NewDefaultPlatform();
    if ( nullptr == platform_ ) {
        throw std::runtime_error("v8 default platform creation failure!");
    }
    // ... initialize platform ...
    ::v8::V8::InitializePlatform(platform_.get());
    if ( false == ::v8::V8::Initialize() ) {
        throw std::runtime_error("v8 ICU default platform initialization failure!");
    }
    // ... we're done ...
    initialized_ = true;
}

/**
 * @brief This method must ( and can only ) be called once to initialize V8 engine.
 *
 * @param a_exec_uri
 * @param a_icu_data_uri
 */
void cc::v8::Singleton::Startup (const char* const a_exec_uri, const char* const a_icu_data_uri)
{
    // ... if already initialized ...
    if ( true == initialized_ ) {
        // ... can't be initialized twice ...
        throw std::runtime_error("v8 singleton already initialized!");
    }
    // ... try to initialize ICU ...
    if ( false == ::v8::V8::InitializeICUDefaultLocation(a_exec_uri, a_icu_data_uri) ) {
        throw std::runtime_error("v8 ICU initialization failure!");
    }
    // ... initialize V8 ...
    ::v8::V8::InitializeExternalStartupData(a_exec_uri);
    // ... create new platform ..
    platform_ = ::v8::platform::NewDefaultPlatform();
    if ( nullptr == platform_ ) {
        throw std::runtime_error("v8 default platform creation failure!");
    }
    // ... initialize platform ...
    ::v8::V8::InitializePlatform(platform_.get());
    if ( false == ::v8::V8::Initialize() ) {
        throw std::runtime_error("v8 ICU default platform initialization failure!");
    }
    // ... we're done ...
    initialized_ = true;
}

/**
 * @brief This method must ( and can only ) be called once to initialize V8 isolation.
 */
void cc::v8::Singleton::Initialize ()
{
    // ... if NOT initialized ...
    if ( false == initialized_ ) {
        throw std::runtime_error("v8 singleton not initialized!");
    }
    // ... if already isolated ...
    if ( nullptr != isolate_ ) {
        throw std::runtime_error("v8 already isolated!");
    }
    // ... create new isolate instance ...
    create_params_.array_buffer_allocator = ::v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_                              = ::v8::Isolate::New(create_params_);
}

/**
 * @brief Call this no longer required to be alive.
 */
void cc::v8::Singleton::Shutdown ()
{
    // ... if not initialized ...
    if ( false == initialized_ ) {
        // ... nothing to shutdown ...
        return;
    }
    // ... release isolate ...
    if ( nullptr != isolate_ ) {
        isolate_->Dispose();
        isolate_ = nullptr;
    }
    // ... dispose v8 engine ...
    ::v8::V8::Dispose();
    // platform_ will be deleted by call to
    if ( NULL != platform_ ) {
#if V8_MAJOR_VERSION >= 10
        ::v8::V8::DisposePlatform();
#else
        ::v8::V8::ShutdownPlatform();
#endif
        platform_ = nullptr;
    }
    // ... release buffer allocator ...
    if ( nullptr != create_params_.array_buffer_allocator ) {
        delete create_params_.array_buffer_allocator;
        create_params_.array_buffer_allocator = nullptr;
    }
    // ... can be reused ...
    initialized_ = false;
}
