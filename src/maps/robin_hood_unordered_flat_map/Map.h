#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "robin-hood-hashing/src/include/robin_hood.h"

static const char* MAP_NAME = "r_h::unordered_flat_map";

template <class Key, class T>
using Map = robin_hood::unordered_flat_map<Key, T, Hash<Key>, std::equal_to<>>;