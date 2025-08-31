#pragma once

#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>

namespace Marcus {

inline Task<> just_void() {
    co_return;
}

template <typename T>
Task<T> just_value(T t) {
    co_return std::move(t);
}

template <typename F, typename... Args>
Task<std::invoke_result_t<F, Args...>> just_invoke(F &&f, Args &&...args) {
    co_return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace Marcus
