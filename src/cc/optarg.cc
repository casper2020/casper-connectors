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

#include <unistd.h> // getopt
#include <getopt.h> // struct option

#include <algorithm> // std::max

/**
 * @brief Default constructor.
 *
 * @param a_name    Program name.
 * @param a_version Program version.
 * @param a_banner  Program banner.
 * @param a_opts
 */
cc::OptArg::OptArg (const char* const a_name, const char* const a_version, const char* const a_banner,
                    const std::initializer_list<cc::OptArg::Opt*>& a_opts)
    : name_(a_name), version_(a_version), banner_(a_banner)
{
    counters_.optional_  = 0;
    counters_.mandatory_ = 0;
    counters_.extra_     = 0;
    for ( auto opt : a_opts ) {
        opts_.push_back(std::move(opt));
    }
    long_ = nullptr;
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
    if ( nullptr != long_ ) {
        delete [] long_;
    }
}

#ifdef __APPLE__
#pragma mark -
#endif

/**
 * @brief Parse arguments.
 *
 * @param a_argc
 * @param a_argv
 * @param a_unknown_argument_callback
 *
 * @return On success 0, -1 on failure.
 */
int cc::OptArg::Parse (const int& a_argc, const char** const a_argv,
                       const cc::OptArg::UnknownArgumentCallback& a_unknown_argument_callback)
{
    counters_.optional_  = 0;
    counters_.mandatory_ = 0;
    counters_.extra_     = 0;
    
    error_ = "";
    
    if ( nullptr != long_ ) {
        delete [] long_;
    }
        
    // ... allocate long options array - longest case all arguments are switches ...
    const size_t m = std::max(opts_.size(), static_cast<size_t>(a_argc)) + 1;

    long_ = new struct option[m];
    for ( size_t idx = 0 ; idx < m ; ++idx ) {
        long_[idx] = { nullptr, 0, nullptr, 0 };
    }

    // ... set getopt format ...
    fmt_ = "";
    for ( size_t idx = 0 ; idx < opts_.size() ; ++idx ) {
        if ( false == opts_[idx]->optional_ ) {
            counters_.mandatory_++;
        } else {
            counters_.optional_++;
        }
        if ( 0 != opts_[idx]->short_ ) {
            fmt_ += opts_[idx]->short_;
        }
        if ( cc::OptArg::Opt::Type::Switch == opts_[idx]->type_ ) {
            long_[idx] = { opts_[idx]->long_.c_str(), no_argument      , nullptr, opts_[idx]->short_ };
        } else {
            fmt_ += ':';
            long_[idx] = { opts_[idx]->long_.c_str(), required_argument, nullptr, opts_[idx]->short_ };
        }
    }

    // ... don't let getopt print errors ...
    opterr = 0;
    
    const auto strip_key = [] (const char* const a_key) -> const char* const {
        const char* ptr = a_key;
        while ( '-' == ptr[0] && '\0' != ptr[0] ) {
            ptr++;
        }
        return ptr;
    };
    
    // ... parse arguments ...
    char opt;
    int  idx;
    while ( -1 != ( opt = getopt_long(a_argc, const_cast<char* const*>(a_argv), fmt_.c_str(), long_, &idx) ) ) {
        // ... search of option ...
        ssize_t rw = -1;
        for ( size_t idx = 0 ; idx < opts_.size() ; ++idx ) {
            if ( opts_[idx]->short_ == opt ) {
                rw = idx;
                break;
            }
        }
        // ... not found?
        if ( -1 == rw ) {
            // ... not a valid option ...
            error_ = "Unrecognized option ";
            // ... not accepting extra argument(s) ...
            error_ += std::string(a_argv[optind-1]);
            // ... we're done ...
            return -1;
        }
        // ... found ...
        switch(opts_[rw]->type_) {
            case cc::OptArg::Opt::Type::Switch:
                dynamic_cast<cc::OptArg::Switch*>(opts_[rw])->Set(1);
                break;
            case cc::OptArg::Opt::Type::String:
                dynamic_cast<cc::OptArg::String*>(opts_[rw])->Set(std::string(static_cast<const char* const>(optarg)));
                break;
            case cc::OptArg::Opt::Type::UInt64:
                dynamic_cast<cc::OptArg::UInt64*>(opts_[rw])->Set(std::stoull(optarg));
                break;
            default:
                throw cc::Exception("Unimplemented type " UINT8_FMT "!", opts_[rw]->type_);
        }
    }

    // ... handle all other non-arguments ...
    if ( nullptr != a_unknown_argument_callback ) {
        for ( ; optind < a_argc ; optind++ ) {
            a_unknown_argument_callback(a_argv[optind], nullptr);
        }
    }
    
    // ... ensure minimum arguments count ...
    if ( static_cast<size_t>(a_argc) < counters_.mandatory_ ) {
        error_ = "Missing or invalid arguments.";
        return -1;
    }
    
    // ... validate opts ...
    for ( auto opt : opts_ ) {
        if ( false == opt->optional_ && false == opt->IsSet() ) {
            error_ = "Missing or invalid option -";
            error_ += opt->short_;
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
    fprintf(stdout, "%s\n", banner_.c_str());
    fprintf(stdout, "\n%s v%s\n", name_.c_str(), version_.c_str());
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
        if ( false == opt->optional_ ) {
            fprintf(stderr, "-%c ", opt->short_);
            if ( cc::OptArg::Opt::Type::Switch != opt->type_ ) {
                fprintf(stderr, "<%s> ", opt->tag_.c_str());
            }
        }
    }
    // ... show optional arguments ( if any ) ...
    if ( counters_.optional_ > 0 ) {
        for ( auto opt : opts_ ) {
            if ( true == opt->optional_ && 0 != opt->short_ ) {
                fprintf(stderr, "[-%c ", opt->short_);
                if ( cc::OptArg::Opt::Type::Switch != opt->type_ ) {
                    fprintf(stderr, "<%s>", opt->tag_.c_str());
                }
                fprintf(stderr, "] ");
            }
        }
    }
    fprintf(stderr, "\n");
    fflush(stderr);
    // ... max long option size ...
    size_t mlos = 0;
    for ( auto opt : opts_ ) {
        if ( opt->long_.length() > mlos ) {
            mlos = opt->long_.length();
        }
    }
    // ... show detailed arguments info ( short options first ) ...
    for ( auto opt : opts_ ) {
        if ( 0 != opt->short_ ) {
            fprintf(stderr, "       -%c, --%--*.*s: %s\n", opt->short_, (int)mlos, (int)mlos, opt->long_.c_str() , opt->help_.c_str());
        }
    }
    // ... show detailed arguments info ( long options last ) ...
    for ( auto opt : opts_ ) {
        if ( 0 == opt->short_ ) {
            fprintf(stderr, "           --%-*.*s: %s\n", (int)mlos, (int)mlos, opt->long_.c_str() , opt->help_.c_str());
        }
    }
    fflush(stderr);
}
