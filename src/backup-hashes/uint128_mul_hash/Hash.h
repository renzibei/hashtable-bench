#pragma once

#include <cstdint>
#include <cstddef>

#if defined(_MSC_VER) && SIZE_MAX == UINT64_MAX
#   include <intrin.h>
#   ifdef _M_X64
#       pragma intrinsic(_umul128)
#   endif
#endif

#include "robin-hood-hashing/src/include/robin_hood.h"

static const char* HASH_NAME = "uint128_mul::hash";

namespace uint128_mul {

    inline size_t MulShiftXor(size_t key, size_t seed) {
#if SIZE_MAX == UINT32_MAX
        uint64_t ret = key;
            ret *= seed;
            return ret ^ (ret >> (sizeof(ret) * 8 / 2));
#else
#   if defined(_MSC_VER)

        uint64_t hi, lo;
#       ifdef _M_X64
            lo = _umul128(key, seed, &hi);
#       else
            lo = key * seed;
            hi = __umulh(key, seed);
#       endif
            return lo ^ hi;
#   else
        __uint128_t ret = key;
        ret *= seed;
        return static_cast<uint64_t>(ret ^ (ret >> (sizeof(ret) * 8 / 2)));
#   endif
#endif
    }

    inline uint64_t MulXorShiftHash64(uint64_t key) {
        constexpr uint64_t kMul = 0x9ddfea08eb382d69ULL;
        return MulShiftXor(key, kMul);
    }

    template<class Key>
    struct hash {
        size_t operator()(const Key& key) const noexcept {
            return MulXorShiftHash64(key);
        }
    };

    template<class CharT>
    struct hash<std::basic_string<CharT>> {
        size_t operator()(const std::basic_string<CharT>&src) const noexcept{
            return robin_hood::hash_bytes(src.data(), src.length());
        }
    };

    template<class CharT>
    struct hash<std::basic_string_view<CharT>> {
        size_t operator()(const std::basic_string_view<CharT>&src) const noexcept{
            return robin_hood::hash_bytes(src.data(), src.length());
        }
    };
} // namespace uint128_mul

template <typename Key>
using Hash = uint128_mul::hash<Key>;