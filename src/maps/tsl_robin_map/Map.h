#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "tsl/robin_map.h"

static const char* MAP_NAME = "tsl::robin_map";

template <class Key, class T>
using Map = tsl::robin_map<Key, T, Hash<Key>, std::equal_to<>, Allocator<Key, T>>;