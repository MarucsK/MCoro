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

struct RepeatAwaitable {
    RepeatAwaiter operator co_await() { return RepeatAwaiter(); }
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

    void return_void() { mRetValue = 0; }

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

    std::coroutine_handle<promise_type> mCoroutine;
};

struct WorldTask {
    using promise_type = Promise;

    WorldTask(std::coroutine_handle<promise_type> coroutine)
        : mCoroutine(coroutine) {}

    WorldTask(WorldTask &&) = delete;

    ~WorldTask() { mCoroutine.destroy(); }

    struct WorldAwaiter {
        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<> coroutine) const noexcept {
            mCoroutine.promise().mPrevious = coroutine;
            return mCoroutine;
        }

        void await_resume() const noexcept {}

        std::coroutine_handle<promise_type> mCoroutine;
    };

    auto operator co_await() { return WorldAwaiter(mCoroutine); }

    std::coroutine_handle<promise_type> mCoroutine;
};

WorldTask world() {
    std::cout << "world" << std::endl;
    co_yield 422;
    co_yield 444;
    co_return;
}

Task hello() {
    std::cout << "hello 正在构建 worldTask" << std::endl;
    WorldTask worldTask = world();
    std::cout << "hello 构建完了worldTask, 开始等待world" << std::endl;
    co_await worldTask;
    std::cout << "hello 得到world返回"
              << worldTask.mCoroutine.promise().mRetValue << std::endl;
    co_await worldTask;
    std::cout << "hello 得到world返回"
              << worldTask.mCoroutine.promise().mRetValue << std::endl;
    std::cout << "hello 42" << std::endl;
    co_yield 42;
    std::cout << "hello 12" << std::endl;
    co_yield 12;
    std::cout << "hello 6" << std::endl;
    co_yield 6;
    std::cout << "hello 结束" << std::endl;
    co_return;
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