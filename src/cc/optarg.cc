/**
* @file optarg.h
*
* Copyright (c) 2011-2020 Cloudware S.A. All rights reserved.
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
* along with casper-connectors.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cc/optarg.h"

#include "cc/types.h"

#include "cc/exception.h"

#include <unistd.h>// getopt

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Default constructor.
 *
 * @param a_name    Program name.
 * @param a_version Program version.
 * @param a_opts
 */
cc::OptArg::OptArg (const char* const a_name, const char* const a_version,
                    const std::initializer_list<cc::OptArg::Opt*>& a_opts)
    : name_(a_name), version_(a_version)
{
    counters_.optional_  = 0;
    counters_.mandatory_ = 0;
    for ( auto opt : a_opts ) {
        opts_.push_back(std::move(opt));
    }
}

/**
 * @brief Destructor.
 */
cc::OptArg::~OptArg ()
{
    for ( auto opt : opts_ ) {
        delete opt;
    }
    opts_.clear();
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Parse arguments.
 *
 * @param a_argc
 * @param a_argv
 *
 * @return On success 0, -1 on failure.
 */
int cc::OptArg::Parse (const int& a_argc, const char** const a_argv)
{
    counters_.optional_  = 0;
    counters_.mandatory_ = 0;

    error_ = "";

    // ... set getopt format ...
    fmt_ = "";
    for ( auto opt : opts_ ) {
        if ( false == opt->optional_ ) {
            counters_.mandatory_++;
        } else {
            counters_.optional_++;
        }
        fmt_ += opt->opt_;
        if ( cc::OptArg::Opt::Type::Switch != opt->type_ ) {
            fmt_ += ':';
        }
    }

    // ... ensure minimum arguments count ...
    if ( static_cast<size_t>(a_argc) < counters_.mandatory_ ) {
        error_ = "Missing or invalid arguments.";
        return -1;
    }
    
    // ... parse arguments ...
    char k;
    while ( -1 != ( k = getopt(a_argc, const_cast<char *const *>(a_argv), fmt_.c_str()) ) ) {
        ssize_t rw = -1;
        for ( size_t idx = 0 ; idx < opts_.size() ; ++idx ) {
            if ( opts_[idx]->opt_ == k ) {
                rw = idx;
                break;
            }
        }
        if ( -1 == rw ) {
            error_ = "Unrecognized option -";
            error_ += k;
            return -1;
        }
        switch(opts_[rw]->type_) {
            case cc::OptArg::Opt::Type::Switch:
                dynamic_cast<cc::OptArg::Switch*>(opts_[rw])->Set(1);
                break;
            case cc::OptArg::Opt::Type::String:
                dynamic_cast<cc::OptArg::String*>(opts_[rw])->Set(optarg);
                break;
            case cc::OptArg::Opt::Type::UInt64:
                dynamic_cast<cc::OptArg::UInt64*>(opts_[rw])->Set(std::stoull(optarg));
                break;
            default:
                throw cc::Exception("Unimplemented type " UINT8_FMT "!", opts_[rw]->type_);
        }
    }
    
    // ... validate opts ...
    for ( auto opt : opts_ ) {
        if ( false == opt->optional_ && false == opt->IsSet() ) {
            error_ = "Missing or invalid option -";
            error_ += opt->opt_;
            error_ += " value!";
            return -1;
        }
    }

    // ... success, done ...
    return 0;
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Show version.
 */
void cc::OptArg::ShowVersion ()
{
    fprintf(stdout, "%s %s\n", name_.c_str(), version_.c_str());
    fflush(stdout);
}

/**
 * @brief Show help.
 */
void cc::OptArg::ShowHelp (const char* const a_message)
{
    if ( nullptr != a_message ) {
        fprintf(stderr, "%s\n", a_message);
    }
    // ... show usage ...
    fprintf(stderr, "usage: %s ", name_.c_str());
    // ... show non-optional arguments ( if any ) ...
    for ( auto opt : opts_ ) {
        if ( true == opt->optional_ ) {
            fprintf(stderr, "-%c <%s> ", opt->opt_, opt->tag_.c_str());
        }
    }
    // ... show optional arguments ( if any ) ...
    if ( counters_.optional_ > 0 ) {
        for ( auto opt : opts_ ) {
            if ( false == opt->optional_ ) {
                fprintf(stderr, "[-%c] ", opt->opt_);
            }
        }
    }
    fprintf(stderr, "\n");
    fflush(stderr);
    // ... show detailed arguments info ...
    for ( auto opt : opts_ ) {
        fprintf(stderr, "       -%c: %s\n", opt->opt_ , opt->help_.c_str());
    }
    fflush(stderr);
}
