#include "Client.h"

#include <chrono>

#include "../Common/ClientHeartbeat.h"

namespace Worms {
    Client::Client(std::string player_name, char const *game_server, uint16_t server_port,
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
              iface_receive_buff{iface_sock} {

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

    void Client::handle_iface_msg() {
        iface_receive_buff.populate();
        auto direction = iface_receive_buff.fetch_direction();
        while (direction.has_value()) {
            turn_direction = *direction;
            direction = iface_receive_buff.fetch_direction();
        }
    }

    void Client::send_heartbeat() {
        server_send_buff.clear();
        ClientHeartbeat heartbeat{session_id, turn_direction, next_expected_event_no,
                                  player_name};
        heartbeat.pack(server_send_buff);
        if (!server_send_buff.flush())
            epoll.watch_fd_for_output(server_sock);
    }

    void Client::handle_events() {
        assert(server_receive_buff.exhausted());
        server_receive_buff.populate();

        uint32_t game_id;
        server_receive_buff.unpack_field(game_id);
        if (game_id != current_game_id &&
            previous_game_ids.find(game_id) == previous_game_ids.end()) {
            if (next_expected_event_no > 0)
                previous_game_ids.insert(current_game_id);
            current_game_id = game_id;
            future_events.clear();
            next_expected_event_no = 0;
        }

        try {
            while (!server_receive_buff.exhausted()) {
                try {
                    auto event = unpack_event(server_receive_buff);

                    if (event->event_no == next_expected_event_no) {
                        ++next_expected_event_no;
                        if (event->event_type != GAME_OVER_NUM) {
                            if (event->event_type == NEW_GAME_NUM) {
                                auto& new_game_ev = *dynamic_cast<Event_NEW_GAME *>(event.get());
                                players = new_game_ev.event_data.players;
                                board_height = new_game_ev.event_data.maxy;
                                board_width = new_game_ev.event_data.maxx;
                            }
                            event->check_validity(players, board_width, board_height);
                            event->stringify(iface_send_buff, players);
                        }
                    } else if (event->event_no > next_expected_event_no) {
                        future_events.insert(std::move(event));
                    } // else discard duplicated event

                    while (!future_events.empty()) { // fetch previously received events from set
                        if ((*future_events.begin())->event_no == next_expected_event_no) {
                            if ((*future_events.begin())->event_type != GAME_OVER_NUM)
                                (*future_events.begin())->stringify(iface_send_buff, players);
                            future_events.erase(future_events.begin());
                        } else {
                            break;
                        }
                    }
                } catch (UnknownEventType const &) {
                    // ignore unknown type
                } catch (BadData const &) {
                    fatal("Valid crc32, yet nonsense data received from server.");
                }

                if (!iface_send_buff.flush()) {
                    epoll.watch_fd_for_output(iface_sock);
                }
            }
        } catch (Crc32Mismatch const &) {
            fputs("Crc32 mismatch!\n", stderr);
            server_receive_buff.discard();
        }
    }

    void Client::play() {
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
}