#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "ska_flat_hash_map/flat_hash_map.hpp"

static const char* MAP_NAME = "ska::flat_hash_map";

template <class Key, class T>
using Map = ska::flat_hash_map<Key, T, Hash<Key>, std::equal_to<>,
        Allocator<std::pair<const Key, T>>>;