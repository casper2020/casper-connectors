/**
 * @file result.h
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
#ifndef NRS_EV_RESULT_H_
#define NRS_EV_RESULT_H_

#include "ev/object.h"
#include "ev/exception.h"

#include <limits> // std::numeric_limits

namespace ev
{
    
    class Result final : public Object
    {
        
    protected: // Data
        
        std::vector<Object*> data_objects_; //!< Attached results, owned by this object. 
        
    public: // Constructor(s) / Destructor
        
        Result (const Object::Target a_target);
        virtual ~Result ();
        
    public: // Method(s) / Function(s)
        
        void          AttachDataObject (Object* a_object, const size_t a_index = std::numeric_limits<size_t>::max());
        Object*       DetachDataObject (const size_t a_index = 0);
        const Object* DataObject       (const size_t a_index = 0) const;
        size_t        DataObjectsCount () const;
        
    public: // Inline Method(s) / Function(s)
        
        const Object* operator[] (int a_index) const;
        
    };
    
    /**
     * @brief Attach a data object, it's memory ownership is now this object responsability.
     *
     * @param a_object
     * @param a_index
     */
    inline void Result::AttachDataObject (Object* a_object, const size_t a_index)
    {
        if ( std::numeric_limits<size_t>::max() == a_index ) {
            data_objects_.push_back(a_object);
        } else if ( a_index >= data_objects_.size() ) {
            throw ev::Exception("Attach index out of bounds!");
        } else {
            data_objects_.insert(data_objects_.begin() + a_index, a_object);
        }
    }

    /**
     * @brief Detach a data object.
     *
     * @param a_index
     *
     * @return The data object, it's memory ownership is now responsability if this method caller.
     */
    inline Object* Result::DetachDataObject (const size_t a_index)
    {
        if ( a_index >= data_objects_.size() ) {
            throw ev::Exception("Detach index out of bounds!");
        }
        Object* rv = data_objects_[a_index];
        data_objects_.erase(data_objects_.begin() + a_index);
        return rv;
    }

    /**
     * @brief Access to a data object by index.
     *
     * @param a_index
     *
     * @return Read only access to a data object for the provided index.
     */
    inline const Object* Result::DataObject (const size_t a_index) const
    {
        if ( a_index >= data_objects_.size() ) {
            throw ev::Exception("Data object access index out of bounds!");
        }
        return data_objects_[a_index];
    }
    
    /**
     * @return The number of stored data objects.
     */
    inline size_t Result::DataObjectsCount () const
    {
        return data_objects_.size();
    }

    /**
     * @brief Operator '[]' overload.
     *
     * @param a_index
     */
    inline const Object* Result::operator[] (int a_index) const
    {
        if ( a_index < 0 || static_cast<size_t>(a_index) >= data_objects_.size() ) {
            throw ev::Exception("Data object access index out of bounds!");
        }
        return data_objects_[static_cast<size_t>(a_index)];
    }
    
} // end of namespace 'ev'

#endif // NRS_EV_RESULT_H_
