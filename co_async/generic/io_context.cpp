#include <co_async/std.hpp>
#include <co_async/awaiter/task.hpp>
#include <co_async/generic/generic_io.hpp>
#include <co_async/generic/io_context.hpp>
#include <co_async/platform/futex.hpp>
#include <co_async/platform/platform_io.hpp>
#include <co_async/utils/cacheline.hpp>

namespace Marcus {

IOContext::IOContext(IOContextOptions options) {
    if (instance) {
        throw std::logic_error("each thread may create only one IOContext");
    }
    instance = this;
    GenericIOContext::instance = &mGenericIO;
    PlatformIOContext::instance = &mPlatformIO;
    if (options.threadAffinity) {
        PlatformIOContext::schedSetThreadAffinity(*options.threadAffinity);
    }
    mPlatformIO.setup(options.queueEntries);
    mMaxSleep = options.maxSleep;
}

IOContext::~IOContext() {
    IOContext::instance = nullptr;
    GenericIOContext::instance = nullptr;
    PlatformIOContext::instance = nullptr;
}

void IOContext::run() {
    while (runOnce())
        ;
}

bool IOContext::runOnce() {
    auto duration = mGenericIO.runDuration();
    if (!duration && !mPlatformIO.hasPendingEvents()) [[unlikely]] {
        return false;
    }
    if (!duration || *duration > mMaxSleep) {
        duration = mMaxSleep;
    }
    mPlatformIO.waitEventsFor(duration);
    return true;
}

thread_local IOContext *IOContext::instance;

} // namespace Marcus
