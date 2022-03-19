// emhash7::HashMap for C++11/14/17
// version 1.7.6
// https://github.com/ktprime/ktprime/blob/master/hash_table7.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2022 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

// From
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution  1 - ln(1-L) - L/2     1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
// separator chain resolution      1 + L / 2             exp(-L) + L

// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0
// SEPARATE CHAN RES.
//    probes/successful lookup    1.05  1.25  1.3   1.25  1.4   1.45  1.50
//    probes/unsuccessful lookup  1.00  1.11  1.15  1.22  1.25  1.31  1.37
//    clacul/unsuccessful lookup  1.01  1.25  1.36, 1.56, 1.64, 1.81, 1.97

/****************
    under random hashCodes, the frequency of nodes in bins follows a Poisson
distribution(http://en.wikipedia.org/wiki/Poisson_distribution) with a parameter of about 0.5
on average for the default resizing threshold of 0.75, although with a large variance because
of resizing granularity. Ignoring variance, the expected occurrences of list size k are
(exp(-0.5) * pow(0.5, k)/factorial(k)). The first values are:
0: 0.60653066
1: 0.30326533
2: 0.07581633
3: 0.01263606
4: 0.00157952
5: 0.00015795
6: 0.00001316
7: 0.00000094
8: 0.00000006

  ============== buckets size ration ========
    1   1543981  0.36884964|0.36787944  36.885
    2    768655  0.36725597|0.36787944  73.611
    3    256236  0.18364065|0.18393972  91.975
    4     64126  0.06127757|0.06131324  98.102
    5     12907  0.01541710|0.01532831  99.644
    6      2050  0.00293841|0.00306566  99.938
    7       310  0.00051840|0.00051094  99.990
    8        49  0.00009365|0.00007299  99.999
    9         4  0.00000860|0.00000913  100.000
========== collision miss ration ===========
  _num_filled aver_size k.v size_kv = 4185936, 1.58, x.x 24
  collision,possion,cache_miss hit_find|hit_miss, load_factor = 36.73%,36.74%,31.31%  1.50|2.00, 1.00
============== buckets size ration ========
*******************************************************/

#pragma once

#include <cstring>
#include <string>
#include <cmath>
#include <cstdlib>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>
#include <algorithm>

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
#elif EMH_WY_HASH
    #include "wyhash.h"
#endif

#ifdef EMH_KEY
    #undef  EMH_KEY
    #undef  EMH_VAL
    #undef  EMH_PKV
    #undef  EMH_NEW
    #undef  EMH_SET
    #undef  EMH_BUCKET
    #undef  EMH_EMPTY
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition)   condition
#    define EMH_UNLIKELY(condition) condition
#endif

#ifndef EMH_BUCKET_INDEX
    #define EMH_BUCKET_INDEX 1
#endif
#if EMH_CACHE_LINE_SIZE < 32
    #define EMH_CACHE_LINE_SIZE 64
#endif

#ifndef EMH_DEFAULT_LOAD_FACTOR
#define EMH_DEFAULT_LOAD_FACTOR 0.88f
#endif
#if EMH_BUCKET_INDEX == 0
    #define EMH_KEY(p,n)     p[n].second.first
    #define EMH_VAL(p,n)     p[n].second.second
    #define EMH_BUCKET(p,n)  p[n].first
    #define EMH_PKV(p,n)     p[n].second
    #define EMH_NEW(key, value, bucket)\
            new(_pairs + bucket) PairT(bucket, value_type(key, value));\
            _num_filled ++;  EMH_SET(bucket)
#elif EMH_BUCKET_INDEX == 2
    #define EMH_KEY(p,n)     p[n].first.first
    #define EMH_VAL(p,n)     p[n].first.second
    #define EMH_BUCKET(p,n)  p[n].second
    #define EMH_PKV(p,n)     p[n].first
    #define EMH_NEW(key, value, bucket)\
            new(_pairs + bucket) PairT(value_type(key, value), bucket);\
            _num_filled ++;  EMH_SET(bucket)
#else
    #define EMH_KEY(p,n)     p[n].first
    #define EMH_VAL(p,n)     p[n].second
    #define EMH_BUCKET(p,n)  p[n].bucket
    #define EMH_PKV(p,n)     p[n]
    #define EMH_NEW(key, value, bucket)\
            new(_pairs + bucket) PairT(key, value, bucket);\
            _num_filled ++; EMH_SET(bucket)
#endif

#define EMH_MASK(bucket)               1 << (bucket % MASK_BIT)
#define EMH_SET(bucket)                _bitmask[bucket / MASK_BIT] &= ~(EMH_MASK(bucket))
#define EMH_CLS(bucket)                _bitmask[bucket / MASK_BIT] |= EMH_MASK(bucket)
#define EMH_EMPTY(bitmask, bucket)     (_bitmask[bucket / MASK_BIT] & (EMH_MASK(bucket))) != 0

#if _WIN32
    #include <intrin.h>
#if _WIN64
    #pragma intrinsic(_umul128)
#endif
#endif

namespace emhash7 {
#ifdef EMH_SIZE_TYPE_16BIT
    typedef uint16_t size_type;
    static constexpr size_type INACTIVE = 0xFFFE;
#elif EMH_SIZE_TYPE_64BIT
    typedef uint64_t size_type;
    static constexpr size_type INACTIVE = 0 - 0x1ull;
#else
    typedef uint32_t size_type;
    static constexpr size_type INACTIVE = 0 - 0x1u;
#endif

static constexpr uint32_t MASK_BIT = sizeof(uint32_t) * 8;
static constexpr uint32_t SIZE_BIT = sizeof(size_t) * 8;
static constexpr uint32_t PACK_SIZE = 2; // > 1
#ifndef EMH_SIZE_TYPE_16BIT
static_assert((int)INACTIVE < 0, "INACTIVE must negative (to int)");
#endif
static_assert(PACK_SIZE > 0, "PACK_SIZE must negative (to int)");

//count the leading zero bit
inline static size_type CTZ(size_t n)
{
#if defined(__x86_64__) || defined(_WIN32) || (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#elif __BIG_ENDIAN__ || (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    n = __builtin_bswap64(n);
#else
    static uint32_t endianness = 0x12345678;
    const auto is_big = *(const char *)&endianness == 0x12;
    if (is_big)
    n = __builtin_bswap64(n);
#endif

#if _WIN32
    unsigned long index;
    #if defined(_WIN64)
    _BitScanForward64(&index, n);
    #else
    _BitScanForward(&index, n);
    #endif
#elif defined (__LP64__) || (SIZE_MAX == UINT64_MAX) || defined (__x86_64__)
    auto index = __builtin_ctzll(n);
#elif 1
    auto index = __builtin_ctzl(n);
#else
    #if defined (__LP64__) || (SIZE_MAX == UINT64_MAX) || defined (__x86_64__)
    size_type index;
    __asm__("bsfq %1, %0\n" : "=r" (index) : "rm" (n) : "cc");
    #else
    size_type index;
    __asm__("bsf %1, %0\n" : "=r" (index) : "rm" (n) : "cc");
    #endif
#endif

    return (size_type)index;
}

template <typename First, typename Second>
struct entry {
    using first_type =  First;
    using second_type = Second;
    entry(const First& key, const Second& value, size_type ibucket)
        :second(value),first(key)
    {
        bucket = ibucket;
    }

    entry(First&& key, Second&& value, size_type ibucket)
        :second(std::move(value)), first(std::move(key))
    {
        bucket = ibucket;
    }

    template<typename K, typename V>
    entry(K&& key, V&& value, size_type ibucket)
        :second(std::forward<V>(value)), first(std::forward<K>(key))
    {
        bucket = ibucket;
    }

    entry(const std::pair<First,Second>& pair)
        :second(pair.second),first(pair.first)
    {
    }

    entry(std::pair<First, Second>&& pair)
        :second(std::move(pair.second)),first(std::move(pair.first))
    {
    }

    entry(const entry& pairt)
        :second(pairt.second),first(pairt.first)
    {
        bucket = pairt.bucket;
    }

    entry(entry&& pairt) noexcept
        :second(std::move(pairt.second)),first(std::move(pairt.first))
    {
        bucket = pairt.bucket;
    }

    entry& operator = (entry&& pairT)
    {
        second = std::move(pairT.second);
        bucket = pairT.bucket;
        first = std::move(pairT.first);
        return *this;
    }

    entry& operator = (const entry& o)
    {
        second = o.second;
        bucket = o.bucket;
        first  = o.first;
        return *this;
    }

    bool operator == (const std::pair<First, Second>& p) const
    {
        return first == p.first && second == p.second;
    }

    bool operator == (const entry<First, Second>& p) const
    {
        return first == p.first && second == p.second;
    }

    void swap(entry<First, Second>& o)
    {
        std::swap(second, o.second);
        std::swap(first, o.first);
    }

#ifndef EMH_ORDER_KV
    Second second;//int
    size_type bucket;
    First first; //long
#else
    First first; //long
    size_type bucket;
    Second second;//int
#endif
};// __attribute__ ((packed));

template <typename A, typename B>
inline constexpr bool operator==(std::pair<A, B> const& x, entry<A, B> const& y) {
    return (x.first == y.first) && (x.second == y.second);
}

template <typename A, typename B>
inline constexpr bool operator==(entry<A, B> const& x, entry<A, B> const& y) {
    return (x.first == y.first) && (x.second == y.second);
}


/// A cache-friendly hash table with open addressing, linear/qua probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
public:
    typedef HashMap<KeyT, ValueT, HashT, EqT> htype;
    typedef std::pair<KeyT,ValueT>            value_type;

#if EMH_BUCKET_INDEX == 0
    typedef value_type                        value_pair;
    typedef std::pair<size_type, value_type>  PairT;
#elif EMH_BUCKET_INDEX == 2
    typedef value_type                        value_pair;
    typedef std::pair<value_type, size_type>  PairT;
#else
    typedef entry<KeyT, ValueT>               value_pair;
    typedef entry<KeyT, ValueT>               PairT;
#endif

    static constexpr bool bInCacheLine = sizeof(value_pair) < (2 * EMH_CACHE_LINE_SIZE) / 3;

    typedef KeyT   key_type;
    typedef ValueT val_type;
    typedef ValueT mapped_type;
    typedef HashT  hasher;
    typedef EqT    key_equal;
    typedef PairT&       reference;
    typedef const PairT& const_reference;

    class const_iterator;
    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        iterator(const const_iterator& it) : _map(it._map), _bucket(it._bucket), _from(it._from), _bmask(it._bmask) { }
        iterator(const htype* hash_map, size_type bucket, bool) : _map(hash_map), _bucket(bucket) { init(); }
        iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init()
        {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _map->bucket_count()) {
                _bmask = *(size_t*)((size_t*)_map->_bitmask + _from / SIZE_BIT);
                _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const
        {
            return _bucket;
        }

        void erase(size_type bucket)
        {
            if (_bucket / SIZE_BIT == bucket / SIZE_BIT)
                _bmask &= ~(1ull << (bucket % SIZE_BIT));
        }

        iterator& next()
        {
            goto_next_element();
            _bmask &= _bmask - 1;
            return *this;
        }

        iterator& operator++()
        {
            _bmask &= _bmask - 1;
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            iterator old = *this;
            _bmask &= _bmask - 1;
            goto_next_element();
            return old;
        }

        reference operator*() const
        {
            return _map->EMH_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PKV(_pairs, _bucket));
        }

        bool operator==(const iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            if (EMH_LIKELY(_bmask != 0)) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = ~*(size_t*)((size_t*)_map->_bitmask + (_from += SIZE_BIT) / SIZE_BIT);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype* _map;
        size_t    _bmask;
        size_type _bucket;
        size_type _from;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef const value_pair*          pointer;
        typedef const value_pair&          reference;

        const_iterator(const iterator& it) : _map(it._map), _bucket(it._bucket), _from(it._from), _bmask(it._bmask) { }
        const_iterator(const htype* hash_map, size_type bucket, bool) : _map(hash_map), _bucket(bucket) { init(); }
        const_iterator(const htype* hash_map, size_type bucket) : _map(hash_map), _bucket(bucket) { _bmask = _from = 0; }

        void init()
        {
            _from = (_bucket / SIZE_BIT) * SIZE_BIT;
            if (_bucket < _map->bucket_count()) {
                _bmask = *(size_t*)((size_t*)_map->_bitmask + _from / SIZE_BIT);
                _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
                _bmask = ~_bmask;
            } else {
                _bmask = 0;
            }
        }

        size_t bucket() const
        {
            return _bucket;
        }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator old(*this);
            goto_next_element();
            return old;
        }

        reference operator*() const
        {
            return _map->EMH_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PKV(_pairs, _bucket));
        }

        bool operator==(const const_iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

    private:
        void goto_next_element()
        {
            _bmask &= _bmask - 1;
            if (EMH_LIKELY(_bmask != 0)) {
                _bucket = _from + CTZ(_bmask);
                return;
            }

            do {
                _bmask = ~*(size_t*)((size_t*)_map->_bitmask + (_from += SIZE_BIT) / SIZE_BIT);
            } while (_bmask == 0);

            _bucket = _from + CTZ(_bmask);
        }

    public:
        const htype* _map;
        size_t    _bmask;
        size_type _bucket;
        size_type _from;
    };

    void init(size_type bucket, float lf = EMH_DEFAULT_LOAD_FACTOR)
    {
        _pairs = nullptr;
        _bitmask = nullptr;
        _num_buckets = _num_filled = 0;
        max_load_factor(lf);
        reserve(bucket);
    }

    HashMap(size_type bucket = 4, float lf = EMH_DEFAULT_LOAD_FACTOR)
    {
        init(bucket, lf);
    }

    inline size_type AllocSize(size_type num_buckets) const
    {
        return num_buckets * sizeof(PairT) + PACK_SIZE * sizeof(PairT) + num_buckets / 8 + sizeof(uint64_t);
    }

    HashMap(const HashMap& other)
    {
        _pairs = (PairT*)malloc(AllocSize(other._num_buckets));
        clone(other);
    }

    HashMap(HashMap&& other)
    {
        _num_buckets = _num_filled = 0;
        _pairs = nullptr;
        swap(other);
    }

    HashMap(std::initializer_list<value_type> ilist)
    {
        init((size_type)ilist.size());
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            do_insert(it->first, it->second);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other)
            return *this;

        if (is_triviall_destructable())
            clearkv();

        if (_num_buckets != other._num_buckets) {
            free(_pairs);
            _pairs = (PairT*)malloc(AllocSize(other._num_buckets));
        }

        clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other)
    {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap()
    {
        if (is_triviall_destructable()) {
            for (auto it = cbegin(); _num_filled; ++it) {
                _num_filled --;
                it->~PairT();
            }
        }
        free(_pairs);
    }

    void clone(const HashMap& other)
    {
        _hasher      = other._hasher;
//        _eq          = other._eq;

        _num_filled  = other._num_filled;
        _mask        = other._mask;
        _loadlf      = other._loadlf;
        _num_buckets = other._num_buckets;

//        _bitmask     = decltype(_bitmask)((uint8_t*)_pairs + ((uint8_t*)other._bitmask - (uint8_t*)other._pairs));
        _bitmask     = decltype(_bitmask)(_pairs + PACK_SIZE + other._num_buckets);

        auto* opairs  = other._pairs;
        if (is_copy_trivially())
            memcpy(_pairs, opairs, AllocSize(_num_buckets));
        else {
            memcpy(_pairs + _num_buckets, opairs + _num_buckets, PACK_SIZE * sizeof(PairT) + _num_buckets / 8 + sizeof(uint64_t));
            for (auto it = other.cbegin(); it.bucket() <= _mask; ++it) {
                const auto bucket = it.bucket();
                EMH_BUCKET(_pairs, bucket) = EMH_BUCKET(opairs, bucket);
                new(_pairs + bucket) PairT(opairs[bucket]);
            }
        }
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher, other._hasher);
        //      std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_loadlf, other._loadlf);
        std::swap(_bitmask, other._bitmask);
        std::swap(EMH_BUCKET(_pairs, _num_buckets), EMH_BUCKET(other._pairs, other._num_buckets));
    }

    // -------------------------------------------------------------
    iterator begin()
    {
        if (0 == _num_filled)
            return {this, _num_buckets};

        const auto bmask = ~(*(size_t*)_bitmask);
        if (bmask != 0)
            return {this, CTZ(bmask), true};

        iterator it(this, sizeof(bmask) * 8 - 1);
        return it.next();
    }

    const_iterator cbegin() const
    {
        const auto bmask = ~(*(size_t*)_bitmask);
        if (bmask != 0)
            return {this, CTZ(bmask), true};
        else if (0 == _num_filled)
            return {this, _num_buckets};

        iterator it(this, sizeof(bmask) * 8 - 1);
        return it.next();
    }

    iterator last() const
    {
        if (_num_filled == 0)
            return end();

        auto bucket = _mask;
        while (EMH_EMPTY(_pairs, bucket)) bucket--;
        return {this, bucket, true};
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return {this, _num_buckets};
    }

    const_iterator cend() const
    {
        return {this, _num_buckets};
    }

    const_iterator end() const
    {
        return cend();
    }

    size_type size() const
    {
        return _num_filled;
    }

    bool empty() const
    {
        return _num_filled == 0;
    }

    // Returns the number of buckets.
    size_type bucket_count() const
    {
        return _num_buckets;
    }

    /// Returns average number of elements per bucket.
    float load_factor() const
    {
        return static_cast<float>(_num_filled) / (_mask + 1);
    }

    HashT& hash_function() const
    {
        return _hasher;
    }

    EqT& key_eq() const
    {
        return _eq;
    }

    constexpr float max_load_factor() const
    {
        return (1 << 27) / (float)_loadlf;
    }

    void max_load_factor(float value)
    {
        if (value < 0.9999f && value > 0.2f)
            _loadlf = (uint32_t)((1 << 27) / value);
    }

    constexpr size_type max_size() const
    {
        return (1ull << (sizeof(size_type) * 8 - 2));
    }

    constexpr size_type max_bucket_count() const
    {
        return max_size();
    }

    size_type bucket_main() const
    {
        auto main_size = 0;
        for (size_type bucket = 0; bucket < _num_buckets; ++bucket) {
            if (EMH_BUCKET(_pairs, bucket) == bucket)
                main_size ++;
        }
        return main_size;
    }


    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key) & _mask;
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (EMH_EMPTY(_pairs, bucket))
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        return (hash_bucket(bucket_key) & _mask) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const size_type bucket) const
    {
        if (EMH_EMPTY(_pairs, bucket))
            return 0;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        next_bucket = hash_bucket(EMH_KEY(_pairs, bucket)) & _mask;
        size_type bucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            bucket_size++;
            next_bucket = nbucket;
        }
        return bucket_size;
    }

    size_type get_main_bucket(const size_type bucket) const
    {
        if (EMH_EMPTY(_pairs, bucket))
            return INACTIVE;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        return main_bucket;
    }

    size_type get_diss(size_type bucket, size_type next_bucket, const size_type slots) const
    {
        auto pbucket = reinterpret_cast<uint64_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<uint64_t>(&_pairs[next_bucket]);
        if (pbucket / EMH_CACHE_LINE_SIZE == pnext / EMH_CACHE_LINE_SIZE)
            return 0;
        size_type diff = pbucket > pnext ? (pbucket - pnext) : (pnext - pbucket);
        if (diff / EMH_CACHE_LINE_SIZE + 1 < slots)
            return (diff / EMH_CACHE_LINE_SIZE + 1);
        return slots - 1;
    }

    int get_bucket_info(const size_type bucket, size_type steps[], const size_type slots) const
    {
        if (EMH_EMPTY(_pairs, bucket))
            return -1;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((hash_bucket(EMH_KEY(_pairs, bucket)) & _mask) != bucket)
            return 0;
        else if (next_bucket == bucket)
            return 1;

        steps[get_diss(bucket, next_bucket, slots)] ++;
        size_type bucket_size = 2;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;

            assert((int)nbucket >= 0);
            steps[get_diss(nbucket, next_bucket, slots)] ++;
            bucket_size ++;
            next_bucket = nbucket;
        }

        return bucket_size;
    }

    void dump_statics(bool show_cache) const
    {
        const int slots = 128;
        size_type buckets[slots + 1] = {0};
        size_type steps[slots + 1]   = {0};
        char buff[1024 * 8];
        for (size_type bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, slots);
            if (bsize >= 0)
                buckets[bsize] ++;
        }

        size_type sumb = 0, sums = 0, sumn = 0;
        size_type miss = 0, finds = 0, bucket_coll = 0;
        double lf = load_factor(), fk = 1.0 / exp(lf), sum_poisson = 0;
        int bsize = sprintf (buff, "============== buckets size ration ========\n");

        miss += _num_buckets - _num_filled;
        for (int i = 1, factorial = 1; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            double poisson = fk / factorial; factorial *= i; fk *= lf;
            if (poisson > 1e-13 && i < 20)
            sum_poisson += poisson * 100.0 * (i - 1) / i;

            const int64_t bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;

            sumb += bucketsi;
            sumn += bucketsi * i;
            bucket_coll += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            miss  += bucketsi * i * i;
            auto errs = (bucketsi * 1.0 * i / _num_filled - poisson) * 100 / poisson;
            bsize += sprintf(buff + bsize, "  %2d  %8ld  %0.8lf|%0.2lf%%  %2.3lf\n",
                    i, bucketsi, bucketsi * 1.0 * i / _num_filled, errs, sumn * 100.0 / _num_filled);
            if (sumn >= _num_filled)
                break;
        }

        bsize += sprintf(buff + bsize, "========== collision miss ration ===========\n");
        for (size_type i = 0; show_cache && i < sizeof(steps) / sizeof(steps[0]); i++) {
            sums += steps[i];
            if (steps[i] == 0)
                continue;
            if (steps[i] > 10)
                bsize += sprintf(buff + bsize, "  %2d  %8u  %0.2lf  %.2lf\n", (int)i, steps[i], steps[i] * 100.0 / bucket_coll, sums * 100.0 / bucket_coll);
        }

        if (sumb == 0)  return;

        bsize += sprintf(buff + bsize, "  _num_filled aver_size k.v size_kv = %u, %.2lf, %s.%s %zd\n",
                _num_filled, _num_filled * 1.0 / sumb, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(PairT));

        bsize += sprintf(buff + bsize, "  collision,poisson,cache_miss hit_find|hit_miss, load_factor = %.2lf%%,%.2lf%%,%.2lf%%  %.2lf|%.2lf, %.2lf\n",
                (bucket_coll * 100.0 / _num_filled), sum_poisson, (bucket_coll - steps[0]) * 100.0 / _num_filled,
                finds * 1.0 / _num_filled, miss * 1.0 / _num_buckets, _num_filled * 1.0 / _num_buckets);

        bsize += sprintf(buff + bsize, "============== buckets size end =============\n");
        buff[bsize + 1] = 0;

#ifdef EMH_LOG
        EMH_LOG() << __FUNCTION__ << "|" << buff << endl;
#else
        puts(buff);
#endif
        assert(sumn == _num_filled);
        assert(sums == bucket_coll || !show_cache);
        assert(bucket_coll == buckets[0]);
    }

    // ------------------------------------------------------------
    template<typename Key = KeyT>
    iterator find(const Key& key, size_t hash_v) noexcept
    {
        return {this, find_filled_hash(key, hash_v)};
    }

    template<typename Key = KeyT>
    const_iterator find(const Key& key, size_t hash_v) const noexcept
    {
        return {this, find_filled_hash(key, hash_v)};
    }

    template<typename Key>
    iterator find(const Key& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    template<typename Key = KeyT>
    const_iterator find(const Key& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    template<typename Key = KeyT>
    ValueT& at(const KeyT& key)
    {
        const auto bucket = find_filled_bucket(key);
        //throw
        return EMH_VAL(_pairs, bucket);
    }

    template<typename Key = KeyT>
    const ValueT& at(const KeyT& key) const
    {
        const auto bucket = find_filled_bucket(key);
        //throw
        return EMH_VAL(_pairs, bucket);
    }

    template<typename Key = KeyT>
    bool contains(const Key& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    template<typename Key = KeyT>
    size_type count(const Key& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets ? 1 : 0;
    }

    template<typename Key = KeyT>
    std::pair<iterator, iterator> equal_range(const Key& key) const noexcept
    {
        const auto found = {this, find_filled_bucket(key), true};
        if (found == end())
            return { found, found };
        else
            return { found, std::next(found) };
    }

    /// Returns false if key isn't found.
    bool try_get(const KeyT& key, ValueT& val) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            val = EMH_VAL(_pairs, bucket);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Const version of the above
    ValueT* try_get(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
    }

    /// Convenience function.
    ValueT get_or_return_default(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? ValueT() : EMH_VAL(_pairs, bucket);
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value)
    {
        reserve(_num_filled);
        return do_insert(key, value);
    }

    std::pair<iterator, bool> insert(KeyT&& key, ValueT&& value)
    {
        reserve(_num_filled);
        return do_insert(std::move(key), std::move(value));
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        reserve(_num_filled);
        return do_insert(key, std::move(value));
    }

    std::pair<iterator, bool> insert(KeyT&& key, const ValueT& value)
    {
        reserve(_num_filled);
        return do_insert(std::move(key), value);
    }

    template<typename K = KeyT, typename V = ValueT>
    inline std::pair<iterator, bool> do_assign(K&& key, V&& value)
    {
        reserve(_num_filled);
        const auto bucket = find_or_allocate(key);
        const auto found = EMH_EMPTY(_pairs, bucket);
        if (found) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket);
        } else {
            EMH_VAL(_pairs, bucket) = std::move(value);
        }
        return { {this, bucket}, found };
    }

    template<typename K = KeyT, typename V = ValueT>
    inline std::pair<iterator, bool> do_insert(K&& key, V&& value)
    {
        const auto bucket = find_or_allocate(key);
        const auto found = EMH_EMPTY(_pairs, bucket);
        if (found) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket);
        }
        return { {this, bucket}, found };
    }

    std::pair<iterator, bool> insert(const value_type& p)
    {
        check_expand_need();
        return do_insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(iterator it, const value_type& p)
    {
        check_expand_need();
        return do_insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(value_type && p)
    {
        check_expand_need();
        return do_insert(std::move(p.first), std::move(p.second));
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve(ilist.size() + _num_filled);
        for (auto it = ilist.begin(); it != ilist.end(); ++it)
            do_insert(it->first, it->second);
    }

    template <typename Iter>
    void insert(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            do_insert(begin->first, begin->second);
        }
    }

#if 0
    template <typename Iter>
    void insert2(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(begin->first, begin->second) == INACTIVE) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg)
            insert(*citbeg);
    }

    size_type try_insert_mainbucket(const KeyT& key, const ValueT& value)
    {
        const auto bucket = hash_bucket(key) & _mask;
        if (!EMH_EMPTY(_pairs, bucket))
            return INACTIVE;

        EMH_NEW(key, value, bucket);
        return bucket;
    }
#endif

    template <typename Iter>
    void insert_unique(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin)
            do_insert_unqiue(*begin);
    }

    /// Same as above, but contains(key) MUST be false
    size_type insert_unique(KeyT&& key, ValueT&& value)
    {
        return do_insert_unqiue(std::move(key), std::move(value));
    }

    size_type insert_unique(const KeyT& key, const ValueT& value)
    {
        return do_insert_unqiue(key, value);
    }

    size_type insert_unique(value_type&& p)
    {
        return do_insert_unqiue(std::move(p.first), std::move(p.second));
    }

    size_type insert_unique(const value_type& p)
    {
        return do_insert_unqiue(p.first, p.second);
    }

    template<typename K, typename V>
    inline size_type do_insert_unqiue(K&& key, V&& value)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket);
        return bucket;
    }

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& value) { return do_assign(key, std::move(value)); }
    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& value) { return do_assign(std::move(key), std::move(value)); }

#if 1
    template <typename... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args)
    {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...);
    }

    inline std::pair<iterator, bool> emplace(const value_type& v)
    {
        check_expand_need();
        return do_insert(v.first, v.second);
    }

    inline std::pair<iterator, bool> emplace(value_type&& v)
    {
        check_expand_need();
        return do_insert(std::move(v.first), std::move(v.second));
    }
#else
    template <class Key, class Val>
    inline std::pair<iterator, bool> emplace(Key&& key, Val&& value)
    {
        return insert(std::move(key), std::move(value));
    }

    template <class Key, class Val>
    inline std::pair<iterator, bool> emplace(const Key& key, const Val& value)
    {
        return insert(key, value);
    }
#endif

    //no any optimize for position
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
        return insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args)
    {
        return insert(k, std::forward<Args>(args)...).first;
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);

        // Check if inserting a new value rather than overwriting an old entry
        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(key, value, bucket);
            return ValueT();
        } else {
            ValueT old_value(value);
            std::swap(EMH_VAL(_pairs, bucket), old_value);
            return old_value;
        }
    }

    /* Check if inserting a new value rather than overwriting an old entry */
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(key, std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    ValueT& operator[](KeyT&& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(std::move(key), std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------
    /// Erase an element from the hash table.
    /// return 0 if element was not found
    template<typename Key = KeyT>
    size_type erase(const Key& key)
    {
        const auto bucket = erase_key(key);
        if (bucket == INACTIVE)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    //iterator erase const_iterator
    iterator erase(const_iterator cit)
    {
        iterator it(cit);
        return erase(it);
    }

    /// Erase an element typedef an iterator.
    /// Returns an iterator to the next element (or end()).
    iterator erase(iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
        it.erase(bucket);
        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? it.next() : it;
    }

    /// Erase an element typedef an iterator without return next iterator
    void _erase(const_iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
    }

    static constexpr bool is_triviall_destructable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_copy_trivially()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clearkv()
    {
        for (auto it = cbegin(); _num_filled; ++it) {
            clear_bucket(it.bucket());
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
        if (is_triviall_destructable())
            clearkv();
        else if (_num_filled > 0)
            memset(_bitmask, 0xFFFFFFFF, _num_buckets / 8);

        EMH_BUCKET(_pairs, _num_buckets) = 0; //_last
        _num_filled = 0;
    }

    void shrink_to_fit()
    {
        rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems)
    {
        const auto required_buckets = (num_elems * _loadlf >> 27);
        if (EMH_LIKELY(required_buckets < _num_buckets))
            return false;

#if EMH_HIGH_LOAD
        if (required_buckets < 64 && _num_filled < _num_buckets)
            return false;
#endif

#if EMH_STATIS
        if (_num_filled > EMH_STATIS) dump_statics(true);
#endif
        rehash(required_buckets + 2);
        return true;
    }

    void rehash(uint64_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return;

        auto num_buckets = _num_filled > (1u << 16) ? (1u << 16) : 8u;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto* new_pairs = (PairT*)malloc(AllocSize(num_buckets));
        //TODO: throwOverflowError
        auto old_num_filled = _num_filled;
        auto old_pairs = _pairs;
        auto* obmask   = _bitmask;

        _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;

        memset((char*)(new_pairs + _num_buckets), 0, sizeof(PairT) * PACK_SIZE);

        const auto num_byte = num_buckets / 8;
        _bitmask     = decltype(_bitmask)(new_pairs + PACK_SIZE + num_buckets);
        memset(_bitmask, 0xFFFFFFFF, num_byte);
        memset(((char*)_bitmask) + num_byte, 0, sizeof(uint64_t));
        assert(num_buckets % 8 == 0);

        _pairs       = new_pairs;
        for (size_type src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
            if (obmask[src_bucket / MASK_BIT] & (EMH_MASK(src_bucket)))
                continue;

            auto& key = EMH_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            EMH_NEW(std::move(key), std::move(EMH_VAL(old_pairs, src_bucket)), bucket);
            if (is_triviall_destructable())
                old_pairs[src_bucket].~PairT();
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
            auto mbucket = bucket_main();
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/aver_size/K.V/pack/ = %u/%2.lf/%s.%s/%zd",
                    _num_filled, double (_num_filled) / mbucket, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]));
#ifdef EMH_LOG
            static uint32_t ihashs = 0;
            EMH_LOG() << "rhash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
#else
            puts(buff);
#endif
        }
#endif

        free(old_pairs);
        assert(old_num_filled == _num_filled);
    }

private:
    // Can we fit another element?
    inline bool check_expand_need()
    {
        return reserve(_num_filled);
    }

    void clear_bucket(size_type bucket)
    {
        EMH_CLS(bucket);
        if (is_triviall_destructable())
            _pairs[bucket].~PairT();
        _num_filled--;
    }

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value, size_type>::type = 0>
    size_type erase_key(const UType& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        if (EMH_EMPTY(_pairs, bucket))
            return INACTIVE;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto eqkey = _eq(key, EMH_KEY(_pairs, bucket));
        if (next_bucket == bucket) {
            return eqkey ? bucket : INACTIVE;
         } else if (eqkey) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (is_copy_trivially())
                EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
            else
                EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));

            EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        }/* else if (EMH_UNLIKELY(bucket != hash_bucket(EMH_KEY(_pairs, bucket)) & _mask))
            return INACTIVE;
        */

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
                EMH_BUCKET(_pairs, prev_bucket) = (nbucket == next_bucket) ? prev_bucket : nbucket;
                return next_bucket;
            }

            if (nbucket == next_bucket)
                break;
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, size_type>::type = 0>
    size_type erase_key(const UType& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        if (EMH_EMPTY(_pairs, bucket))
            return INACTIVE;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == bucket)
            return _eq(key, EMH_KEY(_pairs, bucket)) ? bucket : INACTIVE;
//        else if (bucket != hash_bucket(EMH_KEY(_pairs, bucket)))
//            return INACTIVE;

        //find erase key and swap to last bucket
        size_type prev_bucket = bucket, find_bucket = INACTIVE;
        next_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
                find_bucket = next_bucket;
                if (nbucket == next_bucket) {
                    EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
                    break;
                }
            }
            if (nbucket == next_bucket) {
                if (find_bucket != INACTIVE) {
                    EMH_PKV(_pairs, find_bucket).swap(EMH_PKV(_pairs, nbucket));
//                    EMH_PKV(_pairs, find_bucket) = EMH_PKV(_pairs, nbucket);
                    EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
                    find_bucket = nbucket;
                }
                break;
            }
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return find_bucket;
    }

    size_type erase_bucket(const size_type bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto main_bucket = hash_bucket(EMH_KEY(_pairs, bucket)) & _mask;
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
                if (is_copy_trivially())
                    EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
                else
                    EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
                EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the bucket with this key, or return bucket size
    template<typename K = KeyT>
    size_type find_filled_hash(const K& key, const size_t hashv) const
    {
        const auto bucket = hashv & _mask;
        if (EMH_EMPTY(_pairs, bucket))
            return _num_buckets;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;

        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        return _num_buckets;
    }

    // Find the bucket with this key, or return bucket size
    template<typename K = KeyT>
    size_type find_filled_bucket(const K& key) const
    {
        const auto bucket = hash_bucket(key) & _mask;
        if (EMH_EMPTY(_pairs, bucket))
            return _num_buckets;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
#if 1
        if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;
#elif 0
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;

        else if (_eq(key, EMH_KEY(_pairs, next_bucket)))
            return next_bucket;
        const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
        if (nbucket == next_bucket)
            return _num_buckets;
        next_bucket = nbucket;
#elif 0
        const auto bucket = hash_bucket(key) & _mask;
        if (EMH_EMPTY(_pairs, bucket))
            return _num_buckets;
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == bucket)
            return _num_buckets;
#endif
//        else if (bucket != hash_bucket(bucket_key) & _mask)
//            return _num_buckets;

        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return _num_buckets;
            next_bucket = nbucket;
        }

        return 0;
    }

    //kick out bucket and find empty to occpuy
    //it will break the orgin link and relnik again.
    //before: main_bucket-->prev_bucket --> bucket   --> next_bucket
    //atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket--> next_bucket
    size_type kickout_bucket(const size_type main_bucket, const size_type bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket]));
        if (is_triviall_destructable())
            _pairs[bucket].~PairT();

        if (next_bucket == bucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;

        //set bit
        EMH_SET(new_bucket);
        //clear bit
        EMH_CLS(bucket);
        return bucket;
    }

/*
** inserts a new key into a hash table; first, check whether key's main
** bucket/position is free. If not, check whether colliding node/bucket is in its main
** position or not: if it is not, move colliding bucket to an empty place and
** put new key in its main position; otherwise (colliding bucket is in its main
** position), new key goes to an empty position. ***/

    template<typename Key=KeyT>
    size_type find_or_allocate(const Key& key)
    {
        const auto bucket = hash_bucket(key) & _mask;
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        if (EMH_EMPTY(_pairs, bucket) || _eq(key, bucket_key))
            return bucket;

        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(bucket_key) & _mask;
        if (main_bucket != bucket)
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket == bucket)
            return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

#if EMH_LRU_SET
        auto prev_bucket = bucket;
#endif
        //find next linked bucket and check key, if lru is set then swap current key with prev_bucket
        while (true) {
            if (EMH_UNLIKELY(_eq(key, EMH_KEY(_pairs, next_bucket)))) {
#if EMH_LRU_SET
                EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, prev_bucket));
                return prev_bucket;
#else
                return next_bucket;
#endif
            }

#if EMH_LRU_SET
            prev_bucket = next_bucket;
#endif

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail, TODO link after main bucket?
        const auto new_bucket = find_empty_bucket(next_bucket);// : find_empty_bucket(next_bucketx);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    // key is not in this map. Find a place to put it.
    size_type find_empty_bucket(const size_type bucket_from)
    {
#if __x86_64__ || _M_X64
        const auto boset = bucket_from % 8;
        auto* const start = (uint8_t*)_bitmask + bucket_from / 8;
#else
        const auto boset = bucket_from % SIZE_BIT;
        auto* const start = (size_t*)_bitmask + bucket_from / SIZE_BIT;
#endif

        const auto bmask = *(size_t*)(start) >> boset;
        if (EMH_LIKELY(bmask != 0)) {
            const auto offset = CTZ(bmask);
            //if (EMH_LIKELY(offset < 8 + 256 / sizeof(PairT)) || begin[0] == 0)
            return bucket_from + offset;

            //const auto rerverse_bit = ((begin[0] * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
            //return bucket_from - boset + 7 - CTZ(rerverse_bit);
            //return bucket_from - boset + CTZ(begin[0]);
        }

        const auto qmask = _mask / SIZE_BIT;
        if (1) {
#if 1
            const auto step = (bucket_from + 2 * SIZE_BIT) & qmask;
            const auto bmask2 = *((size_t*)_bitmask + step);
            if (EMH_LIKELY(bmask2 != 0))
                return step * SIZE_BIT + CTZ(bmask2);
#endif
#if 0
            const auto begino = bucket_from - bucket_from % 32;
            const auto beginw = *(size_t*)((uint8_t*)_bitmask + begino / 8);
            if (beginw != 0)
                return begino + CTZ(beginw);//reverse beginw
#endif
        }

        for (size_t s = 2; ; s += 1) { //2.4.7
            auto& _last = EMH_BUCKET(_pairs, _num_buckets);
            const auto bmask2 = *((size_t*)_bitmask + _last);
            if (bmask2 != 0)
                return _last * SIZE_BIT + CTZ(bmask2);
#if 1
            const auto next1 = (qmask / 2 + _last)  & qmask;
//            const auto next1 = qmask - _last;
            const auto bmask1 = *((size_t*)_bitmask + next1);
            if (bmask1 != 0) {
                _last = next1;
                return next1 * SIZE_BIT + CTZ(bmask1);
            }
#endif
            _last = (_last + 1) & qmask;
        }
        return 0;
    }

    size_type find_last_bucket(size_type main_bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    size_type find_prev_bucket(size_type main_bucket, const size_type bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    size_type find_unique_bucket(const KeyT& key)
    {
        const size_type bucket = hash_bucket(key) & _mask;
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (EMH_EMPTY(_pairs, bucket))
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto main_bucket = hash_bucket(EMH_KEY(_pairs, bucket)) & _mask;
        if (EMH_UNLIKELY(main_bucket != bucket))
            return kickout_bucket(main_bucket, bucket);
        else if (next_bucket != bucket)
            next_bucket = find_last_bucket(next_bucket);

        //find a new empty and link it to tail
        return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);
    }

    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    static inline uint64_t hash64(uint64_t key)
    {
#if __SIZEOF_INT128__ && EMH_FIBONACCI_HASH == 1
        __uint128_t r = key; r *= KC;
        return (uint64_t)(r >> 64) + (uint64_t)r;
#elif EMH_FIBONACCI_HASH == 2
        //MurmurHash3Mixer
        uint64_t h = key;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccd;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53;
        h ^= h >> 33;
        return h;
#elif _WIN64 && EMH_FIBONACCI_HASH == 1
        uint64_t high;
        return _umul128(key, KC, &high) + high;
#elif EMH_FIBONACCI_HASH == 3
        auto ror  = (key >> 32) | (key << 32);
        auto low  = key * 0xA24BAED4963EE407ull;
        auto high = ror * 0x9FB21C651E98DF25ull;
        auto mix  = low + high;
        return mix;
#elif EMH_FIBONACCI_HASH == 1
        uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
        return (r >> 32) + r;
#else
        uint64_t x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, size_type>::type = 0>
    inline size_type hash_bucket(const UType key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return hash64(key);
#elif EMH_IDENTITY_HASH
        return key + (key >> (sizeof(UType) * 4));
#elif EMH_WYHASH64
        return wyhash64(key, KC);
#else
        return (size_type)_hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, size_type>::type = 0>
    inline size_type hash_bucket(const UType& key) const
    {
#ifdef WYHASH_LITTLE_ENDIAN
        return wyhash(key.data(), key.size(), key.size());
#else
        return (size_type)_hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value && !std::is_same<UType, std::string>::value, size_type>::type = 0>
    inline size_type hash_bucket(const UType& key) const
    {
#ifdef EMH_FIBONACCI_HASH
        return _hasher(key) * KC;
#else
        return (size_type)_hasher(key);
#endif
    }

private:
    PairT*    _pairs;
    uint32_t* _bitmask;
    HashT     _hasher;
    EqT       _eq;
    size_type _mask;
    size_type _num_buckets;

    size_type _num_filled;
    uint32_t  _loadlf;
};
} // namespace emhash
#if __cplusplus >= 201103L
//template <class Key, class Val> using emhash7 = emhash7::HashMap<Key, Val, std::hash<Key>, std::equal_to<Key>>;
#endif

//TODO
//2. improve rehash and find miss performance(reduce peak memory)
//3. dump or Serialization interface
//4. node hash map support
//5. support load_factor > 1.0
//6. add grow ration
//8. ... https://godbolt.org/
