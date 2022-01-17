#pragma once

#include "robin-hood-hashing/src/include/robin_hood.h"

static const char* HASH_NAME = "uint128_mul::hash";

namespace uint128_mul {
    inline uint64_t MulXorShiftHash64(uint64_t key) {
#ifdef __GNUC__
        using MultType = __uint128_t;
#else
        using MultType = uint64_t;
#endif
        constexpr uint64_t kMul = 0x9ddfea08eb382d69ULL;
        MultType m = key;
        m *= kMul;
        return static_cast<uint64_t>(m ^ (m >> (sizeof(m) * 8 / 2)));
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
} // namespace test

template <typename Key>
using Hash = uint128_mul::hash<Key>;