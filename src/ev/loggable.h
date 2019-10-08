/**
 * @file loggable.h
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
#ifndef NRS_EV_LOGGABLE_H_
#define NRS_EV_LOGGABLE_H_

#include <string>

namespace ev
{
    
    class Loggable
    {
        
    public: // Data Type(s)
        
        class Data
        {
            
        private: // Const Data
            
            const void* owner_ptr_;

        private: // Data
            
            std::string ip_addr_;
            std::string module_;
            std::string tag_;
            size_t      changes_count_;
            
        public: // Constructor(s) / Destructor
            
            /**
             * @brief Default constructor.
             */
            Data ()
                : owner_ptr_(nullptr)
            {
                changes_count_ = 0;
            }
            
            /**
             * @brief Constructor.
             *
             * @param a_owner_ptr
             * @param a_ip_addr
             * @param a_module
             * @param a_tag
             */
            Data (const void* a_owner_ptr, const std::string& a_ip_addr, const std::string& a_module, const std::string& a_tag)
                : owner_ptr_(a_owner_ptr), ip_addr_(a_ip_addr), module_(a_module), tag_(a_tag), changes_count_(0)
            {
                /* empty */
            }
            
            /**
             * @brief Destructor.
             */
            virtual ~Data ()
            {
                /* empty */
            }
            
        public: // Method(s) / Function(s)
            
            /**
             * @brief Update some info.
             *
             * @param a_module
             * @param a_ip_addr
             * @param a_tag
             */
            inline void Update (const std::string& a_module, const std::string& a_ip_addr, const std::string& a_tag)
            {
                module_         = a_module;
                ip_addr_        = a_ip_addr;
                tag_            = a_tag;
                changes_count_ += 1;
            }
            
        public: // Operator(s) Overload
            
            /**
             * @brief Overloaded '=' operator.
             */
            inline void operator = (const Data& a_data)
            {
                owner_ptr_     = a_data.owner_ptr_;
                ip_addr_       = a_data.ip_addr_;
                module_        = a_data.module_;
                tag_           = a_data.tag_;
                changes_count_ = a_data.changes_count_;
            }
            
        public: // Mehtod(s) / Function(s)
            
            inline const void* owner_ptr () const
            {
                return owner_ptr_;
            }
            
            inline void SetIPAddr (const std::string& a_ip_addr)
            {
                ip_addr_ = a_ip_addr;
            }
            
            inline const char* ip_addr () const
            {
                return ip_addr_.c_str();
            }

            inline void SetModule (const std::string& a_module)
            {
                module_ = a_module;
            }

            inline const char* module () const
            {
                return module_.c_str();
            }
            
            inline void SetTag (const std::string& a_tag)
            {
                tag_ = a_tag;
            }
            
            inline const char* tag () const
            {
                return tag_.c_str();
            }
            
            inline bool changed (const size_t a_last) const
            {
                return ( changes_count_ != a_last );
            }

        }; // end of class 'Data';
    
    public: // Constructor(s) / Destructor
        
        virtual ~Loggable ()
        {
            /* empty */
        }
        
    }; // end of class 'Loggable'
    
} // end of namespace 'ev'
    
#endif // NRS_EV_LOGGABLE_H_
