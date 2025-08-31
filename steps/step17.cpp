#include "co_async/and_then.hpp"
#include "co_async/epoll_loop.hpp"
#include "co_async/limit_timeout.hpp"
#include "co_async/task.hpp"
#include "co_async/timer_loop.hpp"
#include "co_async/when_all.hpp"
#include "co_async/when_any.hpp"
#include <cstring>
#include <debug.hpp>
#include <termios.h>
#include <thread>

// main开始之前执行一个
[[gnu::constructor]] static void disable_canon() {
    struct termios tc;
    tcgetattr(STDIN_FILENO, &tc);
    tc.c_lflag &=
        ~ICANON; // 关闭终端的规范模式，按下的每一个键都会被立刻直接发送给程序，即时响应
    tc.c_lflag &=
        ~ECHO; // 关闭终端的回显(echo)功能，按键隐形，不会显示，只有on_draw可以决定屏幕上显示什么
    tcsetattr(STDIN_FILENO, TCSANOW, &tc);
}

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

char map[20][20];
int x = 10;
int y = 10;
int dx = 0;
int dy = 0;
bool running;

void on_key(char c) {
    if (c == 'q') {
        running = false;
    } else if (c == 'a') {
        dx = -1;
        dy = 0;
    } else if (c == 'd') {
        dx = 1;
        dy = 0;
    } else if (c == 'w') {
        dx = 0;
        dy = -1;
    } else if (c == 's') {
        dx = 0;
        dy = 1;
    }
}

void on_time() {
    if (x + dx >= 20 || x + dx < 0 || y + dy >= 20 || y + dy < 0) {
        running = false;
        return;
    }
    x += dx;
    y += dy;
}

void on_draw() {
    std::memset(map, ' ', sizeof(map));
    map[y][x] = '@';
    std::string s = "\x1b[H\x1b[2J\x1b[3J";
    for (int i = 0; i < 20; ++i) {
        s += '#';
    }
    s += '\n';
    for (int i = 0; i < 20; ++i) {
        s += '#';
        for (int j = 0; j < 20; ++j) {
            s += map[i][j];
        }
        s += "#\n";
    }
    for (int i = 0; i < 20; ++i) {
        s += '#';
    }
    s += '\n';
    write(STDOUT_FILENO, s.data(), s.size());
}

Marcus::Task<> async_main() {
    Marcus::AsyncFile file(STDIN_FILENO);
    auto nextTp = std::chrono::system_clock::now();
    running = true;
    while (true) {
        auto res = co_await limit_timeout(timerLoop, read_string(file), nextTp);
        if (res) {
            for (char c: *res) {
                on_key(c);
            }
            on_draw();
        } else {
            on_time();
            if (!running) {
                break;
            }
            on_draw();
            nextTp = std::chrono::system_clock::now() + 500ms;
        }
    }
}

int main() {
    auto t = async_main();
    t.mCoroutine.resume();
    while (true) {
        auto timeout = timerLoop.run();
        auto hasEvent = epollLoop.run(timeout);
        if (!timeout && !hasEvent) {
            break;
        }
    }
    return 0;
}
