#pragma once

#include "robin-hood-hashing/src/include/robin_hood.h"

static const char* HASH_NAME = "robin_hood::hash";

template <typename Key>
using Hash = robin_hood::hash<Key>;