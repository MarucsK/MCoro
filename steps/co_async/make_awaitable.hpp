#pragma once

#include "concepts.hpp"
#include "task.hpp"
#include <utility>

namespace Marcus {

template <Awaitable A>
A &&make_awaitable(A &&a) {
    return std::forward<A>(a);
}

template <class A>
    requires(!Awaitable<A>)
Task<A> make_awaitable(A &&a) {
    co_return std::forward<A>(a);
}

} // namespace Marcus
