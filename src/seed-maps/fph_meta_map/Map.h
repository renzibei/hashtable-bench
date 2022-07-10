#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "fph/meta_fph_table.h"

static const char* MAP_NAME = "fph::MetaFphMap";

using BucketParamType = uint32_t;

template<class Key, class T>
using Map = fph::MetaFphMap<Key, T, Hash<Key>, std::equal_to<>, Allocator<Key, T>, BucketParamType>;