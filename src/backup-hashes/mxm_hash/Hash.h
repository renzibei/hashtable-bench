#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include <functional>

static const char* HASH_NAME = "mxm::hash";

namespace uint128_mul {

    inline size_t MulXorMul64(uint64_t x) {
        x *= 0xbf58476d1ce4e5b9ull;
        x ^= x >> 56;
        x *= 0x94d049bb133111ebull;
        return x;
    }

    inline uint64_t XorMulXorMulXorMul(uint64_t x) {
        x ^= x >> 33U;
        x *= UINT64_C(0xff51afd7ed558ccd);
        x ^= x >> 33U;
        x *= UINT64_C(0xc4ceb9fe1a85ec53);
//        x ^= x >> 33U;
        return x;
    }


    template<class Key>
    struct hash {
        size_t operator()(const Key& key) const noexcept {
//            return MulXorMul64(key);
            return XorMulXorMulXorMul(key);
        }
    };

    template<class CharT>
    struct hash<std::basic_string<CharT>> {
        size_t operator()(const std::basic_string<CharT>&src) const noexcept{
            return std::hash<std::basic_string<CharT>>{}(src);
        }
    };

    template<class CharT>
    struct hash<std::basic_string_view<CharT>> {
        size_t operator()(const std::basic_string_view<CharT>&src) const noexcept{
            return std::hash<std::basic_string_view<CharT>>(src);
        }
    };
} // namespace uint128_mul

template <typename Key>
using Hash = uint128_mul::hash<Key>;