#pragma once

#include <utility>

namespace Marcus {

template <class T = void>
struct NonVoidHelper {
    using Type = T;
};

template <>
struct NonVoidHelper<void> {
    using Type = NonVoidHelper;

    explicit NonVoidHelper() = default;

    template <class T>
    friend constexpr T &&operator,(T &&t, NonVoidHelper) {
        return std::forward<T>(t);
    }

    const char *repr() const noexcept {
        return "NonVoidHelper";
    }
};

} // namespace Marcus
