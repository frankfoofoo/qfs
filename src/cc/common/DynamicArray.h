//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id$
//
// Created 2011/05/17
// Author: Mike Ovsainnikov
//
// Copyright 2011-2012 Quantcast Corp.
//
// This file is part of Kosmos File System (KFS).
//
// Licensed under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
// Dynamic array implementation with no re-allocation / copy. Suitable for
// array with large dynamic size ranges.
//
//----------------------------------------------------------------------------

#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <cstddef>

namespace KFS
{

template<typename T, size_t Log2FirstBufferSize = 7>
class DynamicArray
{
public:
    typedef T           value_type;
    typedef std::size_t size_t;

    DynamicArray()
        : mSize(0),
          mBufferCount(0),
          mLastBufferIdx(0)
    {
        for (size_t i = 0; i < MaxBufferCount(); i++) {
            mBuffersPtr[i] = 0;
        }
    }
    DynamicArray(
        const DynamicArray& inArray)
        : mSize(0),
          mBufferCount(0),
          mLastBufferIdx(0)
        { (*this) = inArray; }
    ~DynamicArray()
        { DynamicArray::Clear(); }
    DynamicArray& operator=(
        const DynamicArray& inArray)
    {
        size_t theBufIdx   = 0;
        size_t theBufSize  = FirstBufSize();
        size_t theCnt      = inArray.mSize;
        while (theCnt > 0) {
            if (theBufIdx >= mBufferCount) {
                mBuffersPtr[mBufferCount++] = new T[theBufSize];
            }
            const T* theSrcPtr = inArray.mBuffersPtr[theBufIdx];
            T*       theDstPtr = mBuffersPtr[theBufIdx];
            const T* theEndPtr = theSrcPtr;
            if (theCnt > theBufSize) {
                theEndPtr += theBufSize;
                theCnt -= theBufSize;
                theBufIdx++;
                theBufSize += theBufSize;
            } else {
                theEndPtr += theCnt;
                theCnt = 0;
            }
            while (theSrcPtr < theEndPtr) {
                *theDstPtr++ = *theSrcPtr++;
            }
        }
        mLastBufferIdx = theBufIdx;
        DeleteBuffers(theBufIdx + 2); // Hysteresis: keep the last one.
        mSize = inArray.mSize;
        return *this;
    }
    void Clear()
    {
        DeleteBuffers(0);
        mSize          = 0;
        mLastBufferIdx = 0;
    }
    size_t GetSize() const
        { return mSize; }
    bool IsEmpty() const
        { return (mSize <= 0); }
    T& operator [](
        size_t inIndex) const
    {
        size_t theIdx     = inIndex;
        size_t theBufSize = FirstBufSize();
        size_t theBufIdx  = 0;
        while (theIdx >= theBufSize) {
            theIdx     -= theBufSize;
            theBufSize += theBufSize;
            theBufIdx++;
        }
        return *(mBuffersPtr[theBufIdx] + theIdx);
    }
    void Swap(
        DynamicArray& inArray)
    {
        for (size_t i = 0; i < MaxBufferCount(); i++) {
            T* const thePtr = inArray.mBuffersPtr[i];
            inArray.mBuffersPtr[i] = mBuffersPtr[i];
            mBuffersPtr[i] = thePtr;
        }
        size_t const theSize = inArray.mSize;
        inArray.mSize = mSize;
        mSize = theSize;
        size_t const theCnt = inArray.mBufferCount;
        inArray.mBufferCount = mBufferCount;
        mBufferCount = theCnt;
        size_t const theIdx = inArray.mLastBufferIdx;
        inArray.mLastBufferIdx = mLastBufferIdx;
        mLastBufferIdx = theIdx;
    }
    T& PushBack(
        const T& inElem)
    {
        if (mLastBufferIdx >= mBufferCount ||
                (Capacity(mLastBufferIdx + 1) <= mSize &&
                    ++mLastBufferIdx >= mBufferCount)) {
            mBuffersPtr[mBufferCount] = new T[BufSize(mBufferCount)];
            mBufferCount++;
        }
        T& theRet = *(mBuffersPtr[mLastBufferIdx] +
            (mSize - Capacity(mLastBufferIdx)));
        theRet = inElem;
        mSize++;
        return theRet;
    }
    size_t PopBack()
    {
        if (mSize <= 0) {
            return mSize;
        }
        mSize--;
        if (mLastBufferIdx > 0 && Capacity(mLastBufferIdx) == mSize) {
            // Hysteresis: keep the last buffer.
            DeleteBuffers(mLastBufferIdx);
            mLastBufferIdx--;
        }
        return mSize;
    }
    T& Front()
        { return *(mBuffersPtr[0]); }
    const T& Front() const
        { return *(mBuffersPtr[0]); }
    T& Back()
    {
        return *(mBuffersPtr[mLastBufferIdx] +
            (mSize - 1 - Capacity(mLastBufferIdx)));
    }
    const T& Back() const
    {
        return *(mBuffersPtr[mLastBufferIdx] +
            (mSize - 1 - Capacity(mLastBufferIdx)));
    }
    void Resize(
        size_t inSize)
    {
        if (inSize <= mSize) {
            RemoveBack(mSize - inSize);
            return;
        }
        size_t theBufSize  = BufSize(mLastBufferIdx);
        size_t theCapacity = Capacity(mLastBufferIdx) +
            (mLastBufferIdx < mBufferCount ? theBufSize : size_t(0));
        while (theCapacity < inSize) {
            if (++mLastBufferIdx >= mBufferCount) {
                mBuffersPtr[mBufferCount++] = new T[theBufSize];
            }
            theCapacity += theBufSize;
            theBufSize  += theBufSize;
        }
        mSize = inSize;
    }
    size_t RemoveBack(
        size_t inCnt)
    {
        if (inCnt <= 0) {
            return mSize;
        }
        size_t theBufIdx = 0;
        if (inCnt >= mSize) {
            mSize = 0;
        } else {
            mSize -= inCnt;
            size_t theBufSize = FirstBufSize();
            size_t theIdx     = mSize;
            while (theBufSize <= theIdx) {
                theIdx     -= theBufSize;
                theBufSize += theBufSize;
                theBufIdx++;
            }
        }
        mLastBufferIdx = theBufIdx;
        DeleteBuffers(theBufIdx + 2); // Hysteresis: keep the last one.
        return mSize;
    }
    template <typename ET>
    class IteratorT
    {
    public:
        typedef DynamicArray<T, Log2FirstBufferSize> DArray;
        IteratorT(
            const DArray& inArray)
            : mIdx(0),
              mBufIdx(0),
              mBufSize(FirstBufSize()),
              mBufPtr(inArray.mBuffersPtr),
              mArray(inArray)
            {}
        ET* Next()
        {
            if (mIdx >= mArray.mSize) {
                return 0;
            }
            if (mBufIdx >= mBufSize) {
                mBufSize += mBufSize;
                mBufIdx = 0;
                mBufPtr++;
            }
            ++mIdx;
            return (*mBufPtr + mBufIdx++);
        }
        bool HasNext() const
            { return (mIdx < mArray.mSize); }
    private:
        typedef typename DArray::value_type BufsT;
        size_t        mIdx;
        size_t        mBufIdx;
        size_t        mBufSize;
        BufsT* const* mBufPtr;
        const DArray& mArray;
    };
    friend class IteratorT<T>;
    friend class IteratorT<const T>;
    typedef IteratorT<T> Iterator;
    typedef IteratorT<const T> ConstIterator;
private:
    size_t mSize;
    size_t mBufferCount;
    size_t mLastBufferIdx;
    T*     mBuffersPtr[sizeof(size_t) * 8 - Log2FirstBufferSize];

    static inline size_t BufSize(
        size_t inIdx)
        { return (size_t(1) << (Log2FirstBufferSize + inIdx)); }
    static inline size_t FirstBufSize()
        { return BufSize(0); }
    static inline size_t Capacity(
        size_t inBufCount)
    {
        return ((size_t(1) << (Log2FirstBufferSize + inBufCount)) -
            (size_t(1) << Log2FirstBufferSize));
    }
    inline size_t MaxBufferCount()
        { return (sizeof(mBuffersPtr) / sizeof(mBuffersPtr[0])); }
    void DeleteBuffers(
        size_t inCnt)
    {
        while (mBufferCount > inCnt) {
            delete [] mBuffersPtr[--mBufferCount];
            mBuffersPtr[mBufferCount] = 0;
        }
    }
};

}

#endif /* DYNAMIC_ARRAY_H */
