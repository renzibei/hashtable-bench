#pragma once

#include "Hash.h"
#include "Allocator.h"
#include <unordered_map>

static const char* MAP_NAME = "std::unordered_map";

template <class Key, class T>
using Map = std::unordered_map<Key, T, Hash<Key>, std::equal_to<>, Allocator<Key, T>>;