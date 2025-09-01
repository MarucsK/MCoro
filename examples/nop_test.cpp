#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std;

static Task<> amain() {
    auto i = co_await fs_nop();
    debug(), "fs_nop result: ", i;
}

int main() {
    co_main(amain());
    return 0;
}
