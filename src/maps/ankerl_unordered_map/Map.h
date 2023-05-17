#pragma once

#include "Hash.h"
#include "Allocator.h"
#include "unordered_dense/include/ankerl/unordered_dense.h"

static const char* MAP_NAME = "ankerl::unordered_dense_map";

template <class Key, class T>
using Map = ankerl::unordered_dense::map<Key, T, Hash<Key>, std::equal_to<>>;