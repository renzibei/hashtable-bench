#pragma once
#include <memory>

template<class Key, class T>
using Allocator = std::allocator<std::pair<const Key, T>>;