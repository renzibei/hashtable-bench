#pragma once

#include <cstdlib>   // posix_memalign
#include <limits>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <new>
#include <memory>

#ifdef __linux__
#   define ENABLE_THP_ALLOC 1
#   include <sys/mman.h> // madvise
#else
#   define ENABLE_THP_ALLOC 0
#endif

#define ENABLE_THP_PRE_FAULTS 0

namespace thp {

    template <typename T> struct ThpAllocator {
        constexpr static std::size_t HUGE_PAGE_SIZE = 1 << 21; // 2 MiB
        constexpr static std::size_t ALLOC_HUGE_PAGE_THRESHOLD = 1 << 16; // 64KB
        using value_type = T;

        ThpAllocator() = default;

        template <class U>
        constexpr ThpAllocator(const ThpAllocator<U> &) noexcept {}

        ThpAllocator(const ThpAllocator&) = default;

        friend bool operator==(const ThpAllocator&, const ThpAllocator&) {
            return true;
        }

        T *allocate(std::size_t n) {
#if ENABLE_THP_ALLOC || ENABLE_THP_PRE_FAULTS
            size_t bytes_num = n * sizeof(T);
#endif
            void *p = nullptr;
#if ENABLE_THP_ALLOC
            if (bytes_num >= ALLOC_HUGE_PAGE_THRESHOLD) {
                auto alloc_ret = posix_memalign(&p, HUGE_PAGE_SIZE, bytes_num);
                if (alloc_ret != 0) {
                    fprintf(stderr, "Error in alloc\n%s", std::strerror(errno));
                    throw std::bad_alloc();
                }
                madvise(p, bytes_num, MADV_HUGEPAGE);
            }
            else {
                p = malloc(bytes_num);
            }
#else
            p = std::allocator<T>{}.allocate(n);
#endif
            if (p == nullptr) {
                throw std::bad_alloc();
            }
#if ENABLE_THP_PRE_FAULTS
            memset(p, 0, bytes_num);
#endif
            return static_cast<T *>(p);
        }

        void deallocate(T *p, std::size_t n) {
#if ENABLE_THP_ALLOC
            (void)n;
            std::free(p);
#else
            std::allocator<T>{}.deallocate(p, n);
#endif
        }
    };

} // namespace thp

template<class Key, class T>
using Allocator = thp::ThpAllocator<std::pair<const Key, T>>;