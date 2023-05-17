#pragma once

#include <cstddef>
#include <string>
#include "xxhash.h"

static const char* HASH_NAME = "xxHash_xxh3";

namespace xxh::detail {

//    size_t MulHashUInt64(uint64_t key, size_t seed) {
//        return key * seed;
//    }

    inline size_t IdentityHash64(uint64_t key, size_t /*seed*/) {
        return key;
    }

    inline uint64_t XXH3HashBytes(const void* src, size_t length, size_t seed) {
        return XXH3_64bits_withSeed(src, length, seed);
    }

    /**
     * Hash 12 bytes, src should have the length of 12 bytes and 8 bytes alignment
     * @param src
     * @param seed
     * @return
     */
    inline uint64_t Hash12Bytes(const void* src, size_t seed) {
        uint64_t v1 = *reinterpret_cast<const uint64_t*>(src);
        uint32_t v2 = *reinterpret_cast<const uint32_t*>(((const uint64_t*)src) + 1);
        v1 *= seed;
        v1 ^= v2;
        return v1;
    }

} // namespace detail

template<class Key>
struct Hash {
    size_t operator()(const Key& key, size_t seed) const noexcept {
        return xxh::detail::IdentityHash64(key, seed);
    }
};

template<class CharT>
struct Hash<std::basic_string<CharT>> {
    size_t operator()(const std::basic_string<CharT>&src, size_t seed) const noexcept {
        return xxh::detail::XXH3HashBytes(src.data(), src.length(), seed);
    }
};

template<class CharT>
struct Hash<std::basic_string_view<CharT>> {
    size_t operator()(const std::basic_string_view<CharT>&src, size_t seed) const noexcept {
        return xxh::detail::XXH3HashBytes(src.data(), src.length(), seed);

    }
};