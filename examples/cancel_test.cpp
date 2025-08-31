#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std::literals;

static Task<int> compute() {
    auto res = co_await co_sleep(200ms);
    debug(), "sleep result", res;
    CancelToken cancel = co_await co_cancel;
    if (cancel.is_canceled()) {
        co_return 0;
    }
    co_return 42;
}

static Task<> amain() {
    auto ret = co_await co_timeout(compute(), 100ms);
    // auto ret = co_await co_timeout(compute(), 1000ms);
    debug(), "compute result", ret;
    co_return;
}

int main() {
    std::setlocale(LC_ALL, "");
    co_main(amain());
}
