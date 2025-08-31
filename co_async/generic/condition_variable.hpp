#pragma once

#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>
#include <co_async/platform/futex.hpp>

namespace Marcus {

struct ConditionVariable {
private:
    FutexAtomic<std::uint32_t> mFutex{};

public:
    Task<Expected<>> wait() {
        std::uint32_t old = mFutex.load(std::memory_order_relaxed);
        do {
            co_await co_await futex_wait(&mFutex, old);
        } while (mFutex.load(std::memory_order_acquire) == old);
        co_return {};
    }

    void notify_one() {
        mFutex.fetch_add(1, std::memory_order_release);
        futex_notify(&mFutex, 1);
    }

    void notify_all() {
        mFutex.fetch_add(1, std::memory_order_release);
        futex_notify(&mFutex, kFutexNotifyAll);
    }

    using Mask = std::uint32_t;

    Task<Expected<>> wait(Mask mask) {
        std::uint32_t old = mFutex.load(std::memory_order_relaxed);
        do {
            co_await co_await futex_wait(&mFutex, old, mask);
        } while (mFutex.load(std::memory_order_acquire) == old);
        co_return {};
    }

    void notify_one(Mask mask) {
        mFutex.fetch_add(1, std::memory_order_release);
        futex_notify(&mFutex, 1, mask);
    }

    void notify_all(Mask mask) {
        mFutex.fetch_add(1, std::memory_order_release);
        futex_notify(&mFutex, kFutexNotifyAll, mask);
    }
};

} // namespace Marcus
