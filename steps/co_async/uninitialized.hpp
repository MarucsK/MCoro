#pragma once

#include "non_void_helper.hpp"
#include <memory>
#include <utility>

namespace Marcus {

template <class T>
struct Uninitialized {
    union {
        T mValue;
    };

    Uninitialized() noexcept {}

    Uninitialized(Uninitialized &&) = delete;

    ~Uninitialized() noexcept {}

    T moveValue() {
        T ret(std::move(mValue));
        mValue.~T();
        return ret;
    }

    template <class... Ts>
    void putValue(Ts &&...args) {
        new (std::addressof(mValue)) T(std::forward<Ts>(args)...);
    }
};

template <>
struct Uninitialized<void> {
    auto moveValue() {
        return NonVoidHelper<>{};
    }

    void putValue(NonVoidHelper<>) {}
};

template <class T>
struct Uninitialized<const T> : Uninitialized<T> {};

template <class T>
struct Uninitialized<T &> : Uninitialized<std::reference_wrapper<T>> {};

template <class T>
struct Uninitialized<T &&> : Uninitialized<T> {};

} // namespace Marcus
