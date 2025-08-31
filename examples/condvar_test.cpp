#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std::literals;

ConditionVariable cv;

static Task<Expected<>> func() {
    (void)co_await co_sleep(1000ms);
    cv.notify_one();
    co_return {};
}

static Task<Expected<>> amain() {
    co_spawn(func());
    auto e = co_await co_timeout(cv.wait(), 800ms);
    debug(), e;
    e = co_await co_timeout(cv.wait(), 800ms);
    debug(), e;
    e = co_await co_timeout(cv.wait(), 800ms);
    debug(), e;
    e = co_await co_timeout(cv.wait(), 800ms);
    debug(), e;
    e = co_await co_timeout(cv.wait(), 800ms);
    debug(), e;
    co_return {};
}

int main() {
    co_main(amain());
    return 0;
}
