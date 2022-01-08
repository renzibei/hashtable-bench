#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "absl/container/node_hash_map.h"

static const char* MAP_NAME = "absl::node_hash_map";

template <class Key, class T>
using Map = absl::node_hash_map<Key, T, Hash<Key>, std::equal_to<>, Allocator<Key, T>>;