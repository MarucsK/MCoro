#include "co_async/and_then.hpp"
#include "co_async/task.hpp"
#include "co_async/timer_loop.hpp"
#include "co_async/when_all.hpp"
#include "co_async/when_any.hpp"
#include <cerrno>
#include <cstring>
#include <debug.hpp>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

int main() {
    int attr = 1;
    ioctl(0, FIONBIO, &attr);

    int epfd = epoll_create1(0);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = 0;
    epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &event);

    while (true) {
        struct epoll_event ebuf[10];
        int res = epoll_wait(epfd, ebuf, 10, 1000);
        if (res == -1) {
            debug(), "epoll error:", strerror(errno);
        }
        if (res == 0) {
            debug(), "epoll timeout, no input within 1s.";
        }
        for (int i = 0; i < res; i++) {
            debug(), "have input!";
            int fd = ebuf[i].data.fd;
            char c;
            while (true) {
                int len = read(fd, &c, 1);
                if (len <= 0) {
                    if (errno == EWOULDBLOCK) {
                        debug(), "read: come on lately~~";
                        break;
                    }
                    debug(), "read error:", strerror(errno);
                }
                debug(), c;
            }
        }
    }
    return 0;
}
