#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std;

[[maybe_unused]] static Expected<int> success() {
    return 42;
}

[[maybe_unused]] static Expected<int> fail() {
    return std::errc::stream_timeout;
}

[[maybe_unused]] static Task<Expected<>> test() {
    co_await success();
    co_await fail();
    co_await success();
    co_return {};
}

[[maybe_unused]] static Task<> amain() {
    auto ret = co_await test();
    debug(), ret;
}

int main() {
    co_main(amain());
    return 0;
}
