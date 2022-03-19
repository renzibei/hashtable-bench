#pragma once

#include "Hash.h"
#include "Allocator.h"
//#include "emhash/hash_table7.hpp"
#include "emhash/emilib2.hpp"

static const char* MAP_NAME = "emlib2::hash_map2";

template <class Key, class T>
//using Map = emhash7::HashMap<Key, T, Hash<Key>, std::equal_to<>>;
using Map = emilib2::HashMap<Key, T, Hash<Key>, std::equal_to<>>;