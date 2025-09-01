#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std;

static Task<Expected<>> amain() {
    OwningStream file = co_await co_await file_open("../examples/read_file.cpp",
                                                    OpenMode::Read);
    String buffer = co_await co_await file.getall();
    co_await co_await stdio().putline(buffer);
    co_return {};
}

int main() {
    co_main(amain());
    return 0;
}
