/**
 * @file optarg.h
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
#ifndef NRS_CC_RAGEL_H_
#define NRS_CC_RAGEL_H_

#define CC_RAGEL_DECLARE_VARS(name, value, size) \
    const char* const name##_c_str = value; \
    int   cs, act; \
    char* ts, *te; \
    char* p   = (char*) name##_c_str; \
    char* pe  = (char*) name##_c_str + size; \
    const char* eof = pe;

#define CC_RAGEL_SILENCE_VARS(machine) \
    (void)(machine##Machine_error); \
    (void)(machine##Machine_en_main); \
    (void)(machine##Machine_first_final); \
    (void)(act); \
    (void)(ts); \
    (void)(te); \
    (void)(eof);

namespace cc
{

    namespace ragel
    {
    
        struct Stack
        {
            
            int  z_;
            int* s_;
            int  t_;
            
            void PrePush ()
            {
                if ( z_ == 0 ) {
                    z_ = 20;
                    s_ = (int*) malloc(sizeof(int) * static_cast<size_t>(z_));
                }
                if ( t_ == z_ -1 ) {
                    z_ *= 2;
                    s_ = (int*) realloc(s_, sizeof(int) * static_cast<size_t>(z_));
                }
            }
            
            Stack ()
            {
                z_ = 0;
                s_ = nullptr;
                t_ = 0;
            }
            
            virtual ~Stack ()
            {
                if ( nullptr != s_ ) {
                    free(s_);
                }
            }
            
        }; // end of struct 'Stack'
    
    } // end of namespace 'ragel'

} // end of namespace 'cc'

#endif // NRS_CC_RAGEL_H_
