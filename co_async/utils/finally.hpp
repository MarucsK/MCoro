#pragma once

#include <co_async/std.hpp>

namespace Marcus {

template <typename F>
struct Finally {
private:
    F func;
    bool enable;

public:
    Finally(std::nullptr_t = nullptr) : enable(false) {}

    Finally(std::convertible_to<F> auto &&func)
        : func(std::forward<decltype(func)>(func)),
          enable(true) {}

    Finally(Finally &&other)
        : func(std::move(other.func)),
          enable(other.enable) {
        other.enable = false;
    }

    Finally &operator=(Finally &&other) {
        if (this != &other) {
            if (enable) {
                func();
            }
            func = std::move(other.func);
            enable = other.enable;
            other.enable = false;
        }
        return *this;
    }

    void reset() {
        if (enable) {
            func();
        }
        enable = false;
    }

    void release() {
        enable = false;
    }

    ~Finally() {
        if (enable) {
            func();
        }
    }
};

template <typename F>
Finally(F &&) -> Finally<std::decay_t<F>>;

} // namespace Marcus
