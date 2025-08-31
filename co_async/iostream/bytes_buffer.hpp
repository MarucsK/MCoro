#pragma once

#include <co_async/std.hpp>
#include <co_async/generic/allocator.hpp>

namespace Marcus {

#if CO_ASYNC_ALLOC
struct BytesBuffer {
private:
    struct Deleter {
        std::size_t mSize;
        std::pmr::memory_resource *mResource;

        void operator()(char *p) noexcept {
            mResource->deallocate(p, mSize);
        }
    };

    std::unique_ptr<char[], Deleter> mData;

public:
    BytesBuffer() noexcept = default;

    explicit BytesBuffer(std::size_t size,
                         std::pmr::polymorphic_allocator<> alloc = {})
        : mData(reinterpret_cast<char *>(alloc.resource()->allocate(size)),
                Deleter{size, alloc.resource()}) {}

    void allocate(std::size_t size,
                  std::pmr::polymorphic_allocator<> alloc = {}) {
        std::pmr::memory_resource *resource = alloc.resource();
        mData.reset(reinterpret_cast<char *>(resource->allocate(size)));
        mData.get_deleter() = {size, resource};
    }

    char *data() const noexcept {
        return mData.get();
    }

    std::size_t size() const noexcept {
        return mData.get_deleter().mSize;
    }

    explicit operator bool() const noexcept {
        return (bool)mData;
    }

    char &operator[](std::size_t index) const noexcept {
        return mData[index];
    }

    operator std::span<char>() const noexcept {
        return {data(), size()};
    }
};
#else
struct BytesBuffer {
private:
    char *mData;
    std::size_t mSize;

    void *pageAlignedAlloc(size_t n) {
        return valloc(n);
    }

    void pageAlignedFree(void *p, size_t) {
        free(p);
    }

public:
    BytesBuffer() noexcept : mData(nullptr), mSize(0) {}

    explicit BytesBuffer(std::size_t size)
        : mData(static_cast<char *>(pageAlignedAlloc(size))),
          mSize(size) {}

    BytesBuffer(BytesBuffer &&other) noexcept
        : mData(other.mData),
          mSize(other.mSize) {
        other.mData = nullptr;
        other.mSize = 0;
    }

    BytesBuffer &operator=(BytesBuffer &&other) noexcept {
        if (this != &other) {
            pageAlignedFree(mData, mSize);
            mData = other.mData;
            mSize = other.mSize;
            other.mData = nullptr;
            other.mSize = 0;
        }
        return *this;
    }

    ~BytesBuffer() noexcept {
        pageAlignedFree(mData, mSize);
    }

    void allocate(std::size_t size) {
        mData = static_cast<char *>(pageAlignedAlloc(size));
        mSize = size;
    }

    char *data() const noexcept {
        return mData;
    }

    std::size_t size() const noexcept {
        return mSize;
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(mData);
    }

    char &operator[](std::size_t index) const noexcept {
        return mData[index];
    }

    operator std::span<char>() const noexcept {
        return {data(), size()};
    }
};
#endif

} // namespace Marcus
