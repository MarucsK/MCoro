#define DEBUG_LEVEL 1
#include <co_async/std.hpp>
#include <co_async/co_async.hpp>

using namespace Marcus;
using namespace std::literals;

extern "C" void *__libc_malloc(size_t size);

static void *my_malloc_hook(size_t n, void *caller) {
    void *p = __libc_malloc(n);
    printf("malloc(%zd) = %p\n", n, p);
    return p;
}

static int malloc_hook_active = 1;

extern "C" void *malloc(size_t size) {
    if (malloc_hook_active) {
        malloc_hook_active = 0;
        void *caller = __builtin_return_address(0);
        void *ret = my_malloc_hook(size, caller);
        malloc_hook_active = 1;
        return ret;
    }
    return __libc_malloc(size);
}

[[gnu::noinline]] static int temp(volatile const int &x) {
    return 0;
}

[[gnu::noinline]] static int resu() {
    return 8;
}

static Task<> test() {
    temp(static_cast<const int &>(resu()));
    co_return;
}

int main() {
    co_main(test());
    return 0;
}
