#pragma once
#include <co_async/std.hpp>

namespace Marcus {

struct current_coroutine_awaiter {
    bool await_ready() const noexcept {
        return false;
    }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) noexcept {
        current_ = coroutine;
        return coroutine;
    }

    auto await_resume() const noexcept {
        return current_;
    }

    std::coroutine_handle<> current_;
};

} // namespace Marcus
