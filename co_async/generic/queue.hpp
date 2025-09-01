#pragma once

#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>
#include <co_async/generic/condition_variable.hpp>
#include <co_async/utils/cacheline.hpp>
#include <co_async/utils/non_void_helper.hpp>
#include <co_async/utils/ring_queue.hpp>
#include <co_async/utils/spin_mutex.hpp>

namespace Marcus {

template <typename T>
struct Queue {
private:
    RingQueue<T> mQueue;
    ConditionVariable mReady;

    static constexpr ConditionVariable::Mask kNonEmptyMask = 1;
    static constexpr ConditionVariable::Mask kNonFullMask = 2;

public:
    explicit Queue(std::size_t size) : mQueue(size) {}

    std::optional<T> try_pop() {
        bool wasFull = mQueue.full();
        auto value = mQueue.pop();
        if (value && wasFull) {
            mReady.notify_one(kNonFullMask);
        }
        return value;
    }

    bool try_push(T &&value) {
        bool wasEmpty = mQueue.empty();
        bool ok = mQueue.push(std::move(value));
        if (ok && wasEmpty) {
            mReady.notify_one(kNonEmptyMask);
        }
        return ok;
    }

    Task<Expected<>> push(T value) {
        while (!mQueue.push(std::move(value))) {
            co_await co_await mReady.wait(kNonFullMask);
        }
        mReady.notify_one(kNonEmptyMask);
        co_return {};
    }

    Task<Expected<T>> pop() {
        while (true) {
            if (auto value = mQueue.pop()) {
                mReady.notify_one(kNonFullMask);
                co_return std::move(*value);
            }
            co_await co_await mReady.wait(kNonEmptyMask);
        }
    }
};

} // namespace Marcus
