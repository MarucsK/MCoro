#pragma once

#include <co_async/std.hpp>
#include <co_async/utils/non_void_helper.hpp>

namespace Marcus {

template <typename A>
concept Awaiter = requires(A a, std::coroutine_handle<> h) {
    {a.await_ready()};
    {a.await_suspend(h)};
    {a.await_resume()};
};

template <typename A>
concept Awaitable = Awaiter<A> || requires(A a) {
    { a.operator co_await() } -> Awaiter;
};

template <typename A>
struct AwaitableTraits {
    using type = A;
};

template <Awaiter A>
struct AwaitableTraits<A> {
    using RetType = decltype(std::declval<A>().await_resume());
    using AvoidRetType = Avoid<RetType>;
    using Type = RetType;
    using AwaiterType = A;
};

template <typename A>
    requires(!Awaiter<A> && Awaitable<A>)
struct AwaitableTraits<A>
    : AwaitableTraits<decltype(std::declval<A>().operator co_await())> {};

template <typename... Ts>
struct TypeList {};

template <typename Last>
struct TypeList<Last> {
    using FirstType = Last;
    using LastType = Last;
};

template <typename First, typename... Ts>
struct TypeList<First, Ts...> {
    using FirstType = First;
    using LastType = typename TypeList<Ts...>::LastType;
};

} // namespace Marcus