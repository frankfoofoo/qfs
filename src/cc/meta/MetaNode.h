/*!
 * $Id$
 *
 * \file MetaNode.h
 * \brief Base class for KFS metadata nodes.
 * \author Blake Lewis (Kosmix Corp.)
 *         Mike Ovsiannikov -- implement pool allocator.
 *
 * Copyright 2008-2012,2016 Quantcast Corporation. All rights reserved.
 * Copyright 2006-2008 Kosmix Corp.
 *
 * This file is part of Kosmos File System (KFS).
 *
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#if !defined(META_NODE_H)
#define META_NODE_H

#include "kfstypes.h"
#include "common/PoolAllocator.h"

#include <iostream>

namespace KFS {

// MetaNode flag values
typedef uint8_t MetaNodeFlagBits;
static const MetaNodeFlagBits META_ROOT = 4; //!< root node
static const MetaNodeFlagBits META_LEVEL1 = 8; //!< children are leaves

/*!
 * \brief base class for both internal and leaf nodes
 */
class MetaNode {
private:
    const uint8_t    nodetype;
    MetaNodeFlagBits flagbits;
    const uint8_t    padd0;
    const uint8_t    padd1;
    int              count; // Belongs to Node, here due alignment.
    friend class Node;
    MetaNode& operator=(const MetaNode&);
    MetaNode(const MetaNode&);
protected:
    MetaNode(MetaType t)
        : nodetype(t), flagbits(0), padd0(0), padd1(0), count(0) { }
    MetaNode(MetaType t, MetaNodeFlagBits f)
        : nodetype(t), flagbits(f), padd0(0), padd1(0), count(0) { }
    ~MetaNode() {}
    template <typename T>
    class Allocator
    {
    public:
        typedef PoolAllocator<
            sizeof(T),         // size_t TItemSize,
            size_t(8)   << 20, // size_t TMinStorageAlloc,
            size_t(128) << 20, // size_t TMaxStorageAlloc,
                // no explicit ~Tree() or cleanup implemented yet.
            false              // bool   TForceCleanupFlag
        > Alloc;
        Allocator() : alloc() {}
        void* allocate() {
            return alloc.Allocate();
        }
        void deallocate(void* ptr) {
            alloc.Deallocate(ptr);
        }
        const Alloc& getPoolAllocator() const {
            return alloc;
        }
    private:
        Alloc alloc;
    };
    template <typename T>
    static Allocator<T>& getAllocator(T* ptr = 0)
    {
        static Allocator<T> allocator;
        return allocator;
    }
    template <typename T>
    static void* allocate(T* type = 0)
    {
        return getAllocator(type).allocate();
    }
    template <typename T>
    static void deallocate(T* ptr)
    {
        getAllocator(ptr).deallocate(ptr);
    }
public:
    void destroy();
    MetaType metaType() const { return MetaType(nodetype); }
    Key key() const;  //!< cons up key value for node
    std::ostream& show(std::ostream& os) const;
    MetaNodeFlagBits flags() const { return flagbits; }
    void setflag(MetaNodeFlagBits bit) { flagbits |= bit; }
    void clearflag(MetaNodeFlagBits bit) { flagbits &= ~bit; }
    bool testflag(MetaNodeFlagBits bit) const { return (flagbits & bit) != 0; }
    template <typename T> static const typename Allocator<T>::Alloc&
    getPoolAllocator(T* type = 0) {
        return getAllocator(type).getPoolAllocator();
    }
};

}

#endif  // !defined(META_NODE_H)
