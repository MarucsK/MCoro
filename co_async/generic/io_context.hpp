#pragma once

#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>
#include <co_async/generic/generic_io.hpp>
#include <co_async/platform/platform_io.hpp>
#include <co_async/utils/cacheline.hpp>

namespace Marcus {

struct IOContextOptions {
    std::chrono::steady_clock::duration maxSleep =
        std::chrono::milliseconds(200);
    std::optional<std::size_t> threadAffinity = std::nullopt;
    std::size_t queueEntries = 512;
};

struct alignas(hardware_destructive_interference_size) IOContext {
private:
    GenericIOContext mGenericIO;
    PlatformIOContext mPlatformIO;
    std::chrono::steady_clock::duration mMaxSleep;

public:
    explicit IOContext(IOContextOptions options = {});
    IOContext(IOContext &&) = delete;
    ~IOContext();

    [[gnu::hot]] void run();
    [[gnu::hot]] bool runOnce();

    static thread_local IOContext *instance;
};

inline Task<> co_catch(Task<Expected<>> task) {
    auto ret = co_await task;
    if (ret.has_error()) {
        std::cerr << ret.error().category().name()
                  << " error: " << ret.error().message() << " ("
                  << ret.error().value() << ")\n";
    }
    co_return;
}

inline void co_main(Task<Expected<>> main) {
    IOContext ctx;
    co_spawn(co_catch(std::move(main)));
    ctx.run();
}

inline void co_main(Task<> main) {
    IOContext ctx;
    co_spawn(std::move(main));
    ctx.run();
}

} // namespace Marcus
