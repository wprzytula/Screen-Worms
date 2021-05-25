#ifndef ROBAKI_CLIENT_H
#define ROBAKI_CLIENT_H

#include <netinet/tcp.h>
#include <sys/fcntl.h>

#include "defs.h"
#include "Epoll.h"
#include "Buffer.h"
#include "ClientHeartbeat.h"
#include "Event.h"

namespace Worms {

    class Client {
    private:
        static constexpr long const COMMUNICATION_INTERVAL = 30'000'000;
        static constexpr long const INITIAL_IFACE_BUFF_CAP = 256;

        uint64_t const session_id;
        std::string const player_name;
        int const server_sock;
        int const iface_sock;
        int const heartbeat_timer;

        Epoll epoll;
        UDPSendBuffer server_send_buff;
        UDPReceiveBuffer server_receive_buff;
        TCPSendBuffer iface_send_buff;
        TCPReceiveBuffer iface_receive_buff;
        uint8_t turn_direction;
        uint32_t next_expected_event_no;
        bool queued_heartbeat;
        std::set<std::unique_ptr<Event>, Event::Comparator> future_events;
        std::vector<std::string> players;


    public:
        Client(std::string player_name, char const *game_server, uint16_t server_port,
               char const *game_iface, uint16_t iface_port)
                : session_id{static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count())},
                  player_name{std::move(player_name)},
                  server_sock{gai_sock_factory(SOCK_DGRAM, game_server, server_port)},
                  iface_sock{gai_sock_factory(SOCK_STREAM, game_iface, iface_port)},
                  heartbeat_timer{timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)},
                  epoll{heartbeat_timer},
                  server_send_buff{server_sock},
                  server_receive_buff{server_sock},
                  iface_send_buff{iface_sock, INITIAL_IFACE_BUFF_CAP},
                  iface_receive_buff{iface_sock},
                  turn_direction{0}, next_expected_event_no{0}, queued_heartbeat{false} {

            if (server_sock < 0 || iface_sock < 0)
                syserr(errno, "opening sockets");
            if (heartbeat_timer < 0)
                syserr(errno, "opening timer fd");

            int optval = 1;
            verify(setsockopt(iface_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)),
                   "setsockopt");

            verify(fcntl(server_sock, F_SETFL, O_NONBLOCK), "fcntl");
            verify(fcntl(iface_sock, F_SETFL, O_NONBLOCK), "fcntl");


            epoll.add_fd(server_sock);
            epoll.add_fd(iface_sock);
            epoll.watch_fd_for_input(heartbeat_timer);
            epoll.watch_fd_for_input(server_sock);
            epoll.watch_fd_for_input(iface_sock);
        }

        ~Client() {
            if (close(server_sock) != 0)
                fputs("Error closing server socket", stderr);
            if (close(iface_sock) != 0)
                fputs("Error closing interface socket", stderr);
            if (close(heartbeat_timer) != 0)
                fputs("Error closing timer fd", stderr);
        }

    private:
        void handle_iface_msg() {
            assert(!iface_receive_buff.has_data());
            iface_receive_buff.populate();
            while (iface_receive_buff.has_data()) {
                turn_direction = iface_receive_buff.fetch_next();
            }
        }

        void send_heartbeat() {
            server_send_buff.clear();
            ClientHeartbeat heartbeat{session_id, turn_direction, next_expected_event_no,
                                      player_name};
            heartbeat.pack(server_send_buff);
            if (!server_send_buff.flush()) {
                queued_heartbeat = true;
                epoll.watch_fd_for_output(server_sock);
            }
        }

        void drain_server_queue() {
            assert(queued_heartbeat);
            queued_heartbeat = false;
            epoll.stop_watching_fd_for_output(server_sock);
            assert(server_send_buff.flush() == server_send_buff.size());
        }

        void handle_events() {
            assert(server_receive_buff.exhausted());
            server_receive_buff.populate();

            uint32_t game_id;
            server_receive_buff.unpack_field(game_id);

            while (!server_receive_buff.exhausted()) {
                auto event = unpack_event(server_receive_buff);

                // TODO: NEW_GAME, GAME_OVER

                printf("Received event no %u\n", event->event_no);

                if (event->event_no == next_expected_event_no) {
                    ++next_expected_event_no;
                    if (event->event_type == GAME_OVER_NUM) {
                        next_expected_event_no = 0;
                    } else {
                        if (event->event_type == NEW_GAME_NUM)
                            players = dynamic_cast<Event_NEW_GAME*>(event.get())->event_data.players;
                        event->stringify(iface_send_buff, players);
                    }
                } else if (event->event_no > next_expected_event_no) {
                    future_events.insert(std::move(event));
                } // else discard duplicated event

                while (!future_events.empty()) { // fetch previously received events from set
                    if ((*future_events.begin())->event_no == next_expected_event_no) {
                        (*future_events.begin())->stringify(iface_send_buff, players);
                        future_events.erase(future_events.begin());
                    } else {
                        break;
                    }
                }
            }

            if (!iface_send_buff.flush()) {
                epoll.watch_fd_for_output(iface_sock);
            }
        }

    public:
        [[noreturn]] void play() {
            {
                struct timespec spec{.tv_sec = 0, .tv_nsec = COMMUNICATION_INTERVAL};
                struct itimerspec conf{.it_interval = spec, .it_value = spec};
                verify(timerfd_settime(heartbeat_timer, 0, &conf, nullptr),
                       "timerfd_settime");
            }
            struct epoll_event event{};
            for (;;) {
                event = epoll.wait();
                if (event.data.fd == heartbeat_timer) {
                    uint64_t expirations;
//                    verify(read(heartbeat_timer, &expirations, sizeof(expirations)),
//                           "read timerfd");
                    read(heartbeat_timer, &expirations, sizeof(expirations));
                    send_heartbeat();
                } else if (event.events & EPOLLOUT) {
                    if (event.data.fd == server_sock) { // drain server queue
                        drain_server_queue();
                    } else { // drain iface queue
                        if (iface_send_buff.flush()) {
                            epoll.stop_watching_fd_for_output(iface_sock);
                        } // else keep us notified about iface socket possibility to send
                    }
                } else {
                    if (event.data.fd == server_sock) {
                        // receive events from server
                        handle_events();
                    } else {
                        // receive pressed/released from interface
                        handle_iface_msg();
                    }
                }
            }
        }
    };
}

#endif //ROBAKI_CLIENT_H
