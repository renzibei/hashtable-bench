#pragma once

#include <functional>


static const char* HASH_NAME = "std::hash";

template <typename Key>
using Hash = std::hash<Key>;