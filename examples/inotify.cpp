#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std;

static Task<Expected<>> amain() {
    auto path = make_path(".");
    auto w = co_await co_await FileWatch()
                 .watch(path, FileWatch::OnWriteFinished, true)
                 .wait();
    co_await co_await stdio().putline(w.path.string());
    co_return {};
}

int main() {
    co_main(amain());
    return 0;
}
