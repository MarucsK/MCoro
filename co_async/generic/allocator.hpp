#pragma once

#include <co_async/std.hpp>

namespace Marcus {

#if CO_ASYNC_ALLOC
using String = std::pmr::string;
#else
using String = std::string;
#endif

/* "hello"_s */
inline String operator""_s(const char *str, size_t len) {
    return String(str, len);
}

extern thread_local std::pmr::memory_resource *currentAllocator;

struct ReplaceAllocator {
    ReplaceAllocator(std::pmr::memory_resource *allocator) {
        lastAllocator = currentAllocator;
        currentAllocator = allocator;
    }

    ReplaceAllocator(ReplaceAllocator &&) = delete;

    ~ReplaceAllocator() {
        currentAllocator = lastAllocator;
    }

private:
    std::pmr::memory_resource *lastAllocator;
};

} // namespace Marcus
