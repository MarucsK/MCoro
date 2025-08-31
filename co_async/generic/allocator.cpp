#include <co_async/std.hpp>
#include <co_async/generic/allocator.hpp>

namespace Marcus {

thread_local std::pmr::memory_resource *currentAllocator =
    std::pmr::new_delete_resource();
#if CO_ASYNC_ALLOC
namespace {

inline struct DefaultResource : std::pmr::memory_resource {
    void *do_allocate(size_t size, size_t align) override {
        return currentAllocator->allocate(size, align);
    }

    void do_deallocate(void *p, size_t size, size_t align) override {
        return currentAllocator->deallocate(p, size, align);
    }

    bool do_is_equal(
        const std::pmr::memory_resource &other) const noexcept override {
        return currentAllocator->is_equal(other);
    }

    DefaultResource() noexcept {
        std::pmr::set_default_resource(this);
    }

    DefaultResource(DefaultResource &&) = delete;
} defaultResource;

} // namespace
#endif

} // namespace Marcus
