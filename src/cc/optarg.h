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
#pragma once
#ifndef NRS_CC_OPTARG_H_
#define NRS_CC_OPTARG_H_

#include "cc/non-copyable.h"
#include "cc/non-movable.h"
#include "cc/exception.h"

#include <string>
#include <vector>
#include <limits> // std::numeric_limits
#include <functional> // std::function

#include <getopt.h> // struct option

namespace cc
{

    //
    // Statistics Class
    //
    class OptArg : public ::cc::NonCopyable, public ::cc::NonMovable
    {
        
    public: // Data Type(s)
        
        class Opt : public ::cc::NonMovable
        {
        
        public: // Enum(s)
                
            enum class Type : uint8_t
            {
                None,
                Switch,
                String,
                UInt64,
                Boolean
            };

        public: // Const Data
                
            const std::string long_;
            const char        short_;
            const Type        type_;
            const bool        optional_;
            const std::string tag_;
            const std::string help_;
            
        protected: // Data
            
            bool set_;

        public: // Constructor(s) / Destructor
        
            Opt () = delete;
            Opt (const char* const a_long, const char a_short, const Type a_type, const bool a_optional, const char* const a_tag, const char* const a_help)
                : long_(a_long), short_(a_short), type_(a_type), optional_(a_optional), tag_(nullptr != a_tag ? a_tag : ""), help_(a_help), set_(false)
            {
                /* empty */
            }
            Opt (const Opt& a_opt)
                : long_(a_opt.long_), short_(a_opt.short_), type_(a_opt.type_), optional_(a_opt.optional_), tag_(a_opt.tag_), help_(a_opt.help_), set_(a_opt.set_)
            {
                /* empty */
            }
            
            virtual ~Opt ()
            {
                /* empty */
            }
        
        public: // Method(s) / Function(s)
          
            inline bool IsSet () const
            {
                return set_;
            }

        }; // end of class 'Opt'
        
    private: // Data Type(s)
        
        template<typename T> class _Opt : public Opt
        {
        public: // Const Data
            
            const T    default_;
            
        private: // Data
            
            T           value_;
            
        public: // Constructor(s) / Destructor
            
            _Opt () = delete;
            _Opt (const char* const a_long, const char a_short,  const bool a_optional, const Type a_type, const T& a_default, const char* const a_tag, const char* const a_help)
                : Opt(a_long, a_short, a_type, a_optional, a_tag, a_help), default_(a_default), value_(default_)
            {
                /* empty */
            }
            _Opt (const _Opt<T>& a_opt)
                : Opt(a_opt), default_(a_opt.default_), value_(a_opt.value_)
            {
                /* empty */
            }
            
            virtual ~_Opt ()
            {
                /* empty */
            }
                                        
        public: // Method(s) / Function(s)
            
            inline void Set (const T& a_value)
            {
                value_ = a_value;
                set_   = true;
            }
            
            inline const T& value () const
            {
                return value_;
            }
            
        }; // end of class '_Opt'
        
    public: // Data Type(s)
        
        class Switch final : public _Opt<uint8_t>
        {
            public: // Constructor(s) / Destructor
            
            Switch () = delete;
            Switch (const char* const a_long, const char a_short, const bool a_optional, const char* const a_help)
                : _Opt<uint8_t>(a_long, a_short, a_optional, _Opt<uint8_t>::Type::Switch, 1, nullptr, a_help)
            {
                /* empty */
            }
            Switch (const char* const a_long, const char a_short, const uint8_t& a_default, const char* const a_help)
                : _Opt<uint8_t>(a_long, a_short, /* a_optional */ true, _Opt<uint8_t>::Type::Switch, a_default, nullptr, a_help)
            {
                /* empty */
            }
            Switch (const Switch& a_switch)
             : _Opt<uint8_t>(a_switch)
            {
                /* empty */
            }
            virtual ~Switch ()
            {
                /* empty */
            }
        };

        class String final : public _Opt<std::string>
        {
        public: // Constructor(s) / Destructor
            
            String () = delete;
            String (const char* const a_long, const char a_short, const bool a_optional, const char* const a_tag, const char* const a_help)
                : _Opt<std::string>(a_long, a_short, a_optional, _Opt<std::string>::Type::String, "", a_tag, a_help)
            {
                /* empty */
            }
            String (const char* const a_long, const char a_short, const std::string& a_default, const char* const a_tag, const char* const a_help)
                : _Opt<std::string>(a_long, a_short, /* a_optional */ true, _Opt<std::string>::Type::String, a_default, a_tag, a_help)
            {
                /* empty */
            }
            String (const String& a_string)
             : _Opt<std::string>(a_string)
            {
                /* empty */
            }
            virtual ~String ()
            {
                /* empty */
            }
        };

        class UInt64 final : public _Opt<uint64_t>
        {
            public: // Constructor(s) / Destructor
            
            UInt64() = delete;
            UInt64(const char* const a_long, const char a_short, const bool a_optional, const char* const a_tag, const char* const a_help)
                : _Opt<uint64_t>(a_long, a_short, a_optional, _Opt<uint64_t>::Type::UInt64, std::numeric_limits<uint64_t>::max(), a_tag, a_help)
            {
                /* empty */
            }
            UInt64(const char* const a_long, const char a_short, const uint64_t& a_default, const char* const a_tag, const char* const a_help)
                : _Opt<uint64_t>(a_long, a_short, /* a_optional */ true, _Opt<uint64_t>::Type::UInt64, a_default, a_tag, a_help)
            {
                /* empty */
            }
            UInt64 (const UInt64& a_uint64)
             : _Opt<uint64_t>(a_uint64)
            {
                /* empty */
            }
            virtual ~UInt64 ()
            {
                /* empty */
            }
        };
        
        class Boolean final : public _Opt<bool>
        {
            public: // Constructor(s) / Destructor
            
            Boolean() = delete;
            Boolean(const char* const a_long, const char a_short, const bool a_optional, const char* const a_tag, const char* const a_help)
                : _Opt<bool>(a_long, a_short, a_optional, _Opt<bool>::Type::Boolean, std::numeric_limits<bool>::max(), a_tag, a_help)
            {
                /* empty */
            }
            Boolean(const char* const a_long, const char a_short, const bool& a_default, const char* const a_tag, const char* const a_help)
                : _Opt<bool>(a_long, a_short, /* a_optional */ true, _Opt<bool>::Type::Boolean, a_default, a_tag, a_help)
            {
                /* empty */
            }
            Boolean (const Boolean& a_bool)
             : _Opt<bool>(a_bool)
            {
                /* empty */
            }
            virtual ~Boolean ()
            {
                /* empty */
            }
        };
        
        typedef std::function<bool(const char* const , const char* const )> UnknownArgumentCallback;
        
    private: // Const Data

        const std::string name_;
        const std::string version_;
        const std::string banner_;
        
    private: // Data Type(s)
        
        typedef struct {
            size_t optional_;
            size_t mandatory_;
            size_t extra_;
        } Counters;
        
    private: // Data

        Counters          counters_;
        std::vector<Opt*> opts_;
        std::string       fmt_;
        struct option*    long_;
        std::string       error_;

    public: // Constructor(s) / Destructor

        OptArg () = delete;
        OptArg (const char* const a_name, const char* const a_version, const char* const a_banner,
                const std::initializer_list<Opt*>& a_opts);
        virtual ~OptArg();

    public: // Method(s) / Function(s)

        int  Parse         (const int& a_argc, const char** const a_argv,
                            const UnknownArgumentCallback& a_unknown_argument_callback = nullptr);

    public: // Method(s) / Function(s)

        void ShowVersion ();
        void ShowHelp    (const char* const a_message = nullptr);
        
    public: // Method(s) / Function(s)
        
        const Opt*         Get       (const char a_short) const;
        const Switch*      GetSwitch (const char a_short) const;
        const String*      GetString (const char a_short) const;
        const UInt64*      GetUInt64 (const char a_short) const;
        
        const std::string& GetStringValueOf  (const char a_short) const;
        const uint64_t&    GetUInt64ValueOf  (const char a_short) const;
        const bool&        GetBooleanValueOf (const char a_short) const;
        
        bool               IsSet     (const char a_short) const;
        const char* const  error     () const;

    }; // end of class 'OptArg'

    /**
     * @param a_short Short option value.
     * @return        True if a_opt matches a previous set previously defined.
     */
    inline bool OptArg::IsSet (const char a_short) const
    {
        for ( auto opt : opts_ ) {
            if ( a_short == opt->short_ ) {
                return ( opt->IsSet() );
            }
        }
        return false;
    }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
    inline const OptArg::Opt* OptArg::Get (const char a_short) const
    {
        for ( size_t idx = 0 ; idx < opts_.size() ; ++idx ) {
            if ( opts_[idx]->short_ == a_short ) {
                return opts_[idx];
            }
        }
        return nullptr;
    }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
     inline const OptArg::Switch* OptArg::GetSwitch (const char a_short) const
     {
         const auto opt = Get(a_short);
         if ( nullptr != opt ) {
             return dynamic_cast<const OptArg::Switch*>(opt);
         }
         return nullptr;
     }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
   inline const OptArg::String* OptArg::GetString (const char a_short) const
   {
       const auto opt = Get(a_short);
       if ( nullptr != opt ) {
           return dynamic_cast<const OptArg::String*>(opt);
       }
       return nullptr;
   }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
    inline const OptArg::UInt64* OptArg::GetUInt64 (const char a_short) const
    {
        const auto opt = Get(a_short);
        if ( nullptr != opt ) {
            return dynamic_cast<const OptArg::UInt64*>(opt);
        }
        return nullptr;
    }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
    inline const std::string& OptArg::GetStringValueOf (const char a_short) const
    {
        const auto opt = Get(a_short);
        if ( nullptr != opt ) {
            if ( true == opt->IsSet() ) {
                return dynamic_cast<const OptArg::String*>(opt)->value();
            } else if ( true == opt->optional_  ){
                return dynamic_cast<const OptArg::String*>(opt)->default_;
            }
        }
        throw ::cc::Exception("Value of argument '%c' is NOT set!", a_short);
    }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
    inline const uint64_t& OptArg::GetUInt64ValueOf (const char a_short) const
    {
        const auto opt = Get(a_short);
        if ( nullptr != opt ) {
            if ( true == opt->IsSet() ) {
                return dynamic_cast<const OptArg::UInt64*>(opt)->value();
            } else if ( true == opt->optional_  ){
                return dynamic_cast<const OptArg::UInt64*>(opt)->default_;
            }
        }
        throw ::cc::Exception("Value of argument '%c' is NOT set!", a_short);
    }

    /**
     * @param a_short Shot option value.
     *
     * @return Read-only pointer to the option, nullptr if not found.
     */
    inline const bool& OptArg::GetBooleanValueOf (const char a_short) const
    {
        const auto opt = Get(a_short);
        if ( nullptr != opt ) {
            if ( true == opt->IsSet() ) {
                return dynamic_cast<const OptArg::Boolean*>(opt)->value();
            } else if ( true == opt->optional_  ){
                return dynamic_cast<const OptArg::Boolean*>(opt)->default_;
            }
        }
        throw ::cc::Exception("Value of argument '%c' is NOT set!", a_short);
    }

    /**
     * @return Last error message, nullptr if none.
     */
    inline const char* const OptArg::error () const
    {
        return ( 0 != error_.length() ? error_.c_str() : nullptr );
    }

} // end of namespace 'cc'

#endif // NRS_CC_OPTARG_H_
