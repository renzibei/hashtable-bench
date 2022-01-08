#pragma once

#include "absl/hash/hash.h"

static const char* HASH_NAME = "absl::Hash";

template <typename Key>
using Hash = absl::Hash<Key>;