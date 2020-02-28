/**
 * @file unwind.cc
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

#include "cc/backtrace/unwind.h"

#include <cxxabi.h>
#include <libunwind.h>
#include <cstdio>
#include <cstdlib>

#include <iomanip>    // std::setfill, std::setw, etc
#include <vector>     // std::vector
#include <cstdarg>    // va_start, va_end, std::va_list
#include <cstddef>    // std::size_t
#include <stdexcept>  // std::runtime_error
#include <sstream>    // std::stringstream
#include <algorithm>  // std::max

/**
 * @brief Destructor
 */
cc::backtrace::Unwind::~Unwind ()
{
    /* empty */
}

/**
 * @brief Backtrace and write unwound call stack.
 *
 * @param o_stream Output stream.
 */
void cc::backtrace::Unwind::Write (std::ostream& o_stream)
{

    unw_cursor_t  cursor;
    unw_context_t context;
    
    // ... initialize cursor to current frame for local unwinding ...
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    char sym[256];
    
    std::stringstream        ss;
    std::vector<std::string> hr_frames;

    // ... unwind frames one by one, going up the frame stack ...
    while ( unw_step(&cursor) > 0 ) {
        
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        
        if ( 0 == pc ) {
            break;
        }
        
        ss.clear(); ss.str("");
        
        ss << '[' << std::setfill(' ') << std::setw(4) << std::dec << hr_frames.size() << "] 0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << pc;
        
        sym[0] = '\0';
        if ( 0 == unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) ) {
            char* nameptr = sym;
            int status;
            char* demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);
            if ( 0 == status ) {
                nameptr = demangled;
            }
            ss << " + 0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << offset << " - " << nameptr;
            std::free(demangled);
        } else {
            ss << "-- error: unable to obtain symbol name for this frame\n";
        }
        
        hr_frames.push_back(ss.str());
        
    }
    
    // ... write human readable frames to stream ...
    for ( auto hr_f : hr_frames ) {
        o_stream << hr_f << '\n';
    }
}
