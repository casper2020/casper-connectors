/**
 * @file numeric_ids.h
 *
 * Copyright (c) 2011-2022 Cloudware S.A. All rights reserved.
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
#ifndef CASPER_CONNECTORS_NUMERIC_IDS_H_
#define CASPER_CONNECTORS_NUMERIC_IDS_H_

#include <set>
#include <queue>

namespace cc
{
    
    template <typename T>
    class NumericIDs : public ::cc::NonCopyable, public ::cc::NonMovable
    {
    
    private: // Data
        
        T             next_;
        std::set<T>   rented_;
        std::queue<T> cached_;
        
    public: // Constructor(s) / Destructor
        
        NumericIDs();
        virtual ~NumericIDs();
        
    public: // Method(s() / Function(s)
        
        T      Rent       ();
        void   Return     (const T a_id);
        bool   IsInUse    (const T a_id) const;
        size_t InUseCount () const;

    }; // end of class 'NumericID'
    
    /**
     * @brief Default constructor.
     */
    template <typename T>
    NumericIDs<T>::NumericIDs ()
    {
        next_ = std::numeric_limits<T>::min();
    }
    
    /**
     * @brief Destructor.
     */
    template <typename T>
    NumericIDs<T>::~NumericIDs ()
    {
        /* empty */
    }
    
    /**
     * @brief 'Rent' an ID.
     *
     * @return ID to use.
     */
    template <typename T>
    inline T NumericIDs<T>::Rent ()
    {
        T rv;
        if ( cached_.size() > 0 ) {
            // ... cached ...
            rv = cached_.front();
            cached_.pop();
            // ... track ...
            rented_.insert(rv);
        } else if ( ( next_ + 1 ) >= std::numeric_limits<T>::max() ) {
            throw ::cc::Exception("Out of numeric IDs - limit %s!", "reached");
        } else {
            // ... next ...
            rv = ++next_;
            // ... track ...
            rented_.insert(rv);
        }
        // ... done ...
        return rv;
    }
    
    /**
     * @brief Return a previously 'rented' ID.
     *
     * @param a_id ID to return.
     */
    template <typename T>
    inline void NumericIDs<T>::Return (const T a_id)
    {
        const auto it = rented_.find(a_id);
        if ( rented_.end() != it ) {
            rented_.erase(it);
            cached_.push(a_id);
        }
        // ... rewind?
        if ( 0 == rented_.size() ) {
            while ( cached_.size() ) {
                cached_.pop();
            }
            next_ = 0;
        }
    }
    
    /**
     * @brief Check if an ID is in use.
     *
     * @return True if ID is in use false othewise.
     */
    template <typename T>
    inline bool NumericIDs<T>::IsInUse (const T a_id) const
    {
        return ( rented_.end() != rented_.find(a_id) );
    }
    
    /**
    * @return Numer of 'rented' IDs.
     */
    template <typename T>
    inline size_t NumericIDs<T>::InUseCount () const
    {
        return rented_.size();
    }
    
} // end of namespace 'cc'

#endif // CASPER_CONNECTORS_NUMERIC_IDS_H_
