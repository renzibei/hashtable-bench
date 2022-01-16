#pragma once

#include <cstddef>
#include <string>
#include "xxhash.h"

static const char* HASH_NAME = "xxHash_xxh3";

namespace xxh::detail {



    inline size_t IdentityHash64(uint64_t key) {
        return key;
    }

    inline uint64_t XXH3HashBytes(const void* src, size_t length) {
        return XXH3_64bits(src, length);
    }


} // namespace detail

template<class Key>
struct Hash {
    size_t operator()(const Key& key) const noexcept {
        return xxh::detail::IdentityHash64(key);
    }
};

template<class CharT>
struct Hash<std::basic_string<CharT>> {
    size_t operator()(const std::basic_string<CharT>&src) const noexcept{
        return xxh::detail::XXH3HashBytes(src.data(), src.length());
    }
};

template<class CharT>
struct Hash<std::basic_string_view<CharT>> {
    size_t operator()(const std::basic_string_view<CharT>&src) const noexcept{
        return xxh::detail::XXH3HashBytes(src.data(), src.length());
    }
};