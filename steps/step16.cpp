#include "co_async/and_then.hpp"
#include "co_async/epoll_loop.hpp"
#include "co_async/task.hpp"
#include "co_async/timer_loop.hpp"
#include "co_async/when_all.hpp"
#include "co_async/when_any.hpp"
#include <debug.hpp>
#include <fcntl.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

Marcus::EpollLoop epollLoop;
Marcus::TimerLoop timerLoop;

Marcus::Task<std::string> read_string(Marcus::AsyncFile &file) {
    co_await Marcus::wait_file_event(epollLoop, file, EPOLLIN);
    std::string s;
    size_t chunk = 8;
    while (true) {
        char c;
        std::size_t exist = s.size();
        s.resize(exist + chunk);
        std::span<char> buffer(s.data() + exist, chunk);
        auto len = Marcus::readFileSync(file, buffer);
        if (len != chunk) {
            s.resize(exist + len);
            break;
        }
        if (chunk < 65536) {
            chunk *= 4;
        }
    }
    co_return s;
}

Marcus::Task<void> async_main() {
    Marcus::AsyncFile file(STDIN_FILENO);
    while (true) {
        auto s = co_await read_string(file);
        debug(), "successfully read", s;
        if (s == "quit\n") {
            break;
        }
    }
}

int main() {
    auto t = async_main();
    t.mCoroutine.resume();
    while (!t.mCoroutine.done()) {
        auto timeout = timerLoop.run();
        epollLoop.run(timeout);
    }
    return 0;
}

/*
auto timeout = timerLoop.run();这里调用timerLoop
的run方法，但是它里面并没有timer，
所以返回一个std::nullopt，传入epollLoop.run(nullopt)，那么他就一直阻塞等待，不会超时返回。

如果timerLoop的run方法确实返回了
一个时间段，代表里面的定时器距离被唤醒还有多久，
然后把这个时间段timeout传给epollLoop.run，让他的epoll最多阻塞这么长时间。
为什么要这么设计？为什么epoll的阻塞时间要等于它？

A: 统一事件循环。

-
只有IO任务，没有定时器任务。timerLoop.run()返回nullopt，epollLoop.run会无限期阻塞，直到有IO时间发生
-
只有定时器任务(5s)，没有IO任务。sleep_for(5s)_promise添加到TimerLoop。main循环开始，timerLoop.run返回duration:5s，
epollLoop.run(5s)。CPU进入睡眠，它在等待两件事：（不存在）IO事件、5s经过。5s后epollLoop超时返回，循环进入下一轮，timerLoop.run
再次调用，发现sleep_for(5s)到期，恢复该协程。
-
有IO任务，也有定时器任务（3s）。timerLoop.run返回3s，epollLoop.run(3s)调用，epoll_wait被调用，最长等待3s。
情况一：IO事件在1s后发生。epoll_wait在1s后被唤醒，epollLoop处理IO事件，恢复挂起的协程。循环进入下一轮，timerLoop.run返回2s，epollLoop.run(2s)
        2s后epollLoop超时返回，下一轮循环timerLoop发现定时器过期，处理该定时器任务。
情况而：3s内没有发生IO事件。epoll_wait等待了3s后超时返回，循环进入下一轮，timerLoop.run被调用，发现3s定时器到期，恢复定时器协程。
*/
