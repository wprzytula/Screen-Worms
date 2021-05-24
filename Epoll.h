#ifndef ROBAKI_EPOLL_H
#define ROBAKI_EPOLL_H

#include "defs.h"

namespace Worms {
    class Epoll {
    private:
        using flags_t = unsigned int;

        int const epoll_fd;
        int const timerfd;
        std::map<int, flags_t> watching;

    public:
        explicit Epoll(int const timerfd) : epoll_fd{epoll_create(1)}, timerfd{timerfd} {
            add_fd(timerfd);
        }

        ~Epoll() {
            if (close(epoll_fd) != 0)
                fputs("Error closing epoll epoll_fd", stderr);
        }

        void add_fd(int const fd) {
            assert(watching.find(fd) == watching.end());
            watching[fd] = 0;
            struct epoll_event events{
                .events = 0,
                .data{.fd = fd}
            };
            verify(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &events),
                    "epoll add fd");
        }

    private:
        void modify_watching(int const fd) {
            struct epoll_event event{
                    .events = watching[fd],
                    .data{.fd = fd}
            };
            verify(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event), "epoll_ctl");
        }

    public:
        void watch_fd_for_input(int const fd) {
            assert(watching.find(fd) != watching.end());
            assert(!(watching[fd] & EPOLLIN));
            watching[fd] |= EPOLLIN;
            modify_watching(fd);
        }

        void stop_watching_fd_for_input(int const fd) {
            assert(watching.find(fd) != watching.end());
            assert(watching[fd] & EPOLLIN);
            watching[fd] &= ~EPOLLIN;
            modify_watching(fd);
        }

        void watch_fd_for_output(int const fd) {
            assert(watching.find(fd) != watching.end());
//            assert(!(watching[fd] & EPOLLOUT));
            if (watching[fd] & EPOLLOUT)
                return;
            watching[fd] |= EPOLLOUT;
            modify_watching(fd);
        }

        void stop_watching_fd_for_output(int const fd) {
            assert(watching.find(fd) != watching.end());
            assert(watching[fd] & EPOLLOUT);
            watching[fd] &= ~EPOLLOUT;
            modify_watching(fd);
        }

        struct epoll_event wait(int const timeout = -1) {
            int max_events = static_cast<int>(watching.size());
            struct epoll_event events[max_events];

            verify(epoll_wait(epoll_fd, events, max_events, timeout), "epoll_wait");

            for (int i = 0; i < max_events; ++i) {
                if (events[i].data.fd == timerfd)
                    return events[i];
            }
            return events[0];
        }
    };
}

#endif //ROBAKI_EPOLL_H
