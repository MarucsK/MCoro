#include <coroutine>
#include <iostream>

struct RepeatAwaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if ( coroutine.done() ) {
            return std::noop_coroutine();
        } else {
            return coroutine;
        }
    }

    void await_resume() const noexcept {}
};

struct PreviousAwaiter {
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<>
    await_suspend(std::coroutine_handle<> coroutine) const noexcept {
        if ( mPrevious ) {
            return mPrevious;
        } else {
            return std::noop_coroutine();
        }
    }

    void await_resume() const noexcept {}

    std::coroutine_handle<> mPrevious;
};

struct Promise {
    auto initial_suspend() { return std::suspend_always(); }

    auto final_suspend() noexcept { return PreviousAwaiter(mPrevious); }

    void unhandled_exception() { throw; }

    auto yield_value(int ret) {
        mRetValue = ret;
        return std::suspend_always();
    }

    void return_value(int ret) { mRetValue = ret; }

    std::coroutine_handle<Promise> get_return_object() {
        return std::coroutine_handle<Promise>::from_promise(*this);
    }

    int mRetValue;
    std::coroutine_handle<> mPrevious = nullptr;
};

struct Task {
    using promise_type = Promise;

    Task(std::coroutine_handle<promise_type> coroutine)
        : mCoroutine(coroutine) {}

    Task(Task &&) = delete;

    ~Task() { mCoroutine.destroy(); }

    struct Awaiter {
        bool await_ready() const { return false; }

        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> coroutine) const {
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }

        auto await_resume() const { return mCoroutine.promise().mRetValue; }

        std::coroutine_handle<promise_type> mCoroutine;
    };

    auto operator co_await() const { return Awaiter(mCoroutine); }

    std::coroutine_handle<promise_type> mCoroutine;
};

Task world() {
    std::cout << "world" << std::endl;
    co_return 41;
}

Task hello() {
    int i = co_await world();
    std::cout << "hello得到world结果为" << i << std::endl;
    co_return i + 1;
}

int main() {
    std::cout << "main即将调用hello" << std::endl;
    Task t = hello();
    std::cout << "main调用完了hello" << std::endl;
    while ( !t.mCoroutine.done() ) {
        t.mCoroutine.resume();
        std::cout << "main得到hello结果为" << t.mCoroutine.promise().mRetValue
                  << std::endl;
    }
    return 0;
}