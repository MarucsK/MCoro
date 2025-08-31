#pragma once

#include <co_async/std.hpp>
#include <co_async/utils/non_void_helper.hpp>

namespace Marcus {

template <typename T, typename E = void>
struct GeneratorResult {
    /*
    T: co_yield
    E: co_return
    */
    std::variant<T, E> mValue;

    explicit GeneratorResult(std::in_place_index_t<0>, auto &&...args)
        : mValue(std::in_place_index<0>,
                 std::forward<decltype(args)>(args)...) {}

    explicit GeneratorResult(std::in_place_index_t<1>, auto &&...args)
        : mValue(std::in_place_index<1>,
                 std ::forward<decltype(args)>(args)...) {}

    bool has_result() const noexcept {
        return mValue.index() == 1;
    }

    bool has_value() const noexcept {
        return mValue.index() == 0;
    }

    explicit operator bool() const noexcept {
        return has_value();
    }

    T &operator*() & noexcept {
        return *std::get_if<0>(&mValue);
    }

    T &&operator*() && noexcept {
        return std::move(*std::get_if<0>(&mValue));
    }

    const T &operator*() const & noexcept {
        return *std::get_if<0>(&mValue);
    }

    const T &&operator*() const && noexcept {
        return std::move(*std::get_if<0>(&mValue));
    }

    T &operator->() noexcept {
        return std::get_if<0>(&mValue);
    }

    T &value() & {
        return std::get<0>(mValue);
    }

    T &&value() && {
        return std::move(std::get<0>(mValue));
    }

    const T &value() const & {
        return std::get<0>(mValue);
    }

    const T &&value() const && {
        return std::move(std::get<0>(mValue));
    }

    E &result_unsafe() & noexcept {
        return *std::get_if<1>(&mValue);
    }

    E &&result_unsafe() && noexcept {
        return std::move(*std::get_if<1>(&mValue));
    }

    const E &result_unsafe() const & noexcept {
        return *std::get_if<1>(&mValue);
    }

    const E &&result_unsafe() const && noexcept {
        return std::move(*std::get_if<1>(&mValue));
    }

    E &result() & {
        return std::get<1>(mValue);
    }

    E &&result() && {
        return std::move(std::get<1>(mValue));
    }

    const E &result() const & {
        return std::get<1>(mValue);
    }

    const E &&result() const && {
        return std::move(std::get<1>(mValue));
    }
};

template <typename T>
struct GeneratorResult<T, void> : GeneratorResult<T, Void> {
    using GeneratorResult<T, Void>::GeneratorResult;
};

} // namespace Marcus
