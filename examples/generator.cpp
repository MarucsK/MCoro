#define DEBUG_LEVEL 1
#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std;

[[mayby_unused]] static Task<GeneratorResult<int, Expected<>>> task1() {
    for (int i = 9; i <= 99; ++i) {
        co_await co_await co_sleep(300ms);
        co_yield i;
    }
    co_return {};
}

static Task<Expected<>> amain() {
    auto g = task1();
    while (auto r = co_await co_await g) {
        debug(), r;
    }
    debug(), "end";
    co_return {};
}

int main() {
    co_main(amain());
    return 0;
}
