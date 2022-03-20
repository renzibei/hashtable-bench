#pragma once

#include "Hash.h"
#include "Allocator.h"
// define EMH_BUCKET_INDEX for the compiling for hash_table7
#define EMH_BUCKET_INDEX 2
#include "emhash/hash_table7.hpp"
//#include "emhash/emilib2.hpp"

static const char* MAP_NAME = "emhash::hash_map7";
//static const char* MAP_NAME = "emlib::hash_map2";

template <class Key, class T>
using Map = emhash7::HashMap<Key, T, Hash<Key>, std::equal_to<>>;
//using Map = emilib2::HashMap<Key, T, Hash<Key>, std::equal_to<>>;