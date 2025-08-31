#include <co_async/std.hpp>
#include <co_async/generic/generic_io.hpp>

namespace Marcus {

GenericIOContext::GenericIOContext() = default;
GenericIOContext::~GenericIOContext() = default;

std::optional<std::chrono::steady_clock::duration>
GenericIOContext::runDuration() {
    while (true) {
        if (!mTimers.empty()) {
            auto &promise = mTimers.front();
            std::chrono::steady_clock::time_point now =
                std::chrono::steady_clock::now();
            if (promise.mExpires <= now) {
                promise.mCancelled = false;
                promise.erase_from_parent();
                std::coroutine_handle<TimerNode>::from_promise(promise)
                    .resume();
                continue;
            } else {
                return promise.mExpires - now;
            }
        } else {
            return std::nullopt;
        }
    }
}

} // namespace Marcus
