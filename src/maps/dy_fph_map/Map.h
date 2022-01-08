#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "fph/dynamic_fph_table.h"

static const char* MAP_NAME = "fph::DynamicFphMap";

using BucketParamType = uint32_t;

template<class Key, class T>
using Map = fph::DynamicFphMap<Key, T, Hash<Key>, std::equal_to<>, Allocator<Key, T>, BucketParamType>;