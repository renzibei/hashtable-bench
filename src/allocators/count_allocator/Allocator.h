#pragma once

#include <cstdlib>   // posix_memalign
#include <limits>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <new>
#include <algorithm>
#include <memory>

#define USE_COUNT_ALLOC

namespace count {

    template<typename T>
    class CountAllocator;

    class MemoryCount {
    private:
        MemoryCount(): cur_bytes_(0), peak_bytes_(0) {};
        size_t cur_bytes_;
        size_t peak_bytes_;
    public:
        MemoryCount(const MemoryCount&) = delete;
        MemoryCount& operator=(const MemoryCount&) = delete;
        MemoryCount(MemoryCount&&) = delete;
        MemoryCount& operator=(MemoryCount&&) = delete;

        template<class T> friend class CountAllocator;

        static MemoryCount& instance() {
            static MemoryCount _instance;
            return _instance;
        }

        void ResetPeakBytes() {
            peak_bytes_ = 0;
        }

        size_t cur_bytes() const {
            return cur_bytes_;
        }

        size_t peak_bytes() const {
            return peak_bytes_;
        }

    private:
        // This two methods should only be called by
        void UseMemory(size_t bytes) {
            cur_bytes_ += bytes;
            peak_bytes_ = std::max(cur_bytes_, peak_bytes_);
        }

        void ReclaimMemory(size_t bytes) {
            cur_bytes_ -= bytes;
        }

    };

    template <typename T>
    class CountAllocator {
    public:
        constexpr static std::size_t HUGE_PAGE_SIZE = 1 << 21; // 2 MiB
        constexpr static std::size_t ALLOC_HUGE_PAGE_THRESHOLD = 1 << 16; // 64KB
        using value_type = T;

        CountAllocator() = default;

        template <class U>
        constexpr CountAllocator(const CountAllocator<U> &) noexcept {}

        CountAllocator(const CountAllocator&) = default;

        friend bool operator==(const CountAllocator&, const CountAllocator&) {
            return true;
        }

        T *allocate(std::size_t n) {
            // may throw
            void *p = std::allocator<T>{}.allocate(n);
            size_t used_bytes = n * sizeof(T);
            MemoryCount::instance().UseMemory(used_bytes);
            return static_cast<T *>(p);
        }

        void deallocate(T *p, std::size_t n) {
            std::allocator<T>{}.deallocate(p, n);
            size_t bytes_num = n * sizeof(T);
            MemoryCount::instance().ReclaimMemory(bytes_num);
//            if (bytes_num > (1UL << 16U)) {
//                printf("Deallocate %zu bytes\n", bytes_num);
//            }

        }
    };

} // namespace count

template<class T>
using Allocator = count::CountAllocator<T>;