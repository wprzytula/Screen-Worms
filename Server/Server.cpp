#include "Server.h"

namespace Worms {
    Server::Server(uint16_t const port, uint32_t const seed, Worms::GameConstants constants)
            : sock{socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)},
              round_timer{timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)},
              epoll{round_timer},
              rand{RandomGenerator{seed}},
              constants{constants},
              round_duration_ns{NS_IN_SEC / constants.round_per_sec},
              receive_buff{sock} {
        if (sock < 0)
            syserr(errno, "opening socket");
        if (round_timer < 0)
            syserr(errno, "opening timer fd");

        struct sockaddr_in6 server_address{};
        server_address.sin6_family = AF_INET6;
        server_address.sin6_addr = in6addr_any;
        server_address.sin6_port = htobe16(port);

        verify(bind(sock, (struct sockaddr *) &server_address,
                    sizeof(server_address)), "bind");

        verify(fcntl(sock, F_SETFL, O_NONBLOCK), "fcntl");

        epoll.add_fd(sock);
        epoll.watch_fd_for_input(round_timer);
        epoll.watch_fd_for_input(sock);
    }

    void Server::disconnect_idles() {
        std::vector<decltype(connected_clients.begin())> to_disconnect;
        for (auto it = connected_clients.begin(); it != connected_clients.end(); ++it) {
            if ((round_no - (*it)->last_heartbeat_round_no) * round_duration_ns
                >= DISCONNECT_THRESHOLD) {
                to_disconnect.push_back(it);
            }
        }
        for (auto it : to_disconnect) {
            disconnect_client(it);
        }
    }

    void Server::round_routine() {
        if (current_game.has_value()) {
            current_game->play_round();
            current_game->disseminate_new_events(send_queue, sock);
            if (current_game->finished()) {
                previous_game.emplace(std::move(current_game.value()));
                current_game.reset();
            }
        }

        ++round_no;

        if (!drain_queue())
            epoll.watch_fd_for_output(sock);
    }

    void Server::try_start_game() {
        // Check if a game can be started.
        if (connected_players.size() >= 2) {
            bool all_ready = true;
            for (auto& player: connected_players) {
                if (!player->is_ready()) {
                    all_ready = false;
                    break;
                }
            }
            if (all_ready) { // start the game!
                std::vector<std::weak_ptr<Player>> observers;
                for (auto &observer: connected_unnames) {
                    observers.push_back(std::weak_ptr<Player>{observer});
                }
                current_game.emplace(constants, rand, connected_players, std::move(observers));

                if (!drain_queue())
                    epoll.watch_fd_for_output(sock);
            }
        }
    }

    void Server::handle_heartbeat() {
        auto sender = receive_buff.populate();
        try {
            // The following construction may fail with BadData
            // if client sent us invalid heartbeat.
            ClientHeartbeat heartbeat{receive_buff};

            if (heartbeat.player_name.size() > 20)
                return; // player name too long

            if (!std::all_of(heartbeat.player_name.begin(), heartbeat.player_name.end(),
                             [](char const c) -> bool {
                                 return c >= 33 && c <= 126;
                             })) {
                return; // invalid characters in player name
            }

            auto client_ptr = connected_clients.find(sender);
            if (connected_clients.find(sender) == connected_clients.end()) {
                if (player_names.find(heartbeat.player_name) == player_names.end()) {
                    connect_client(sender, std::move(heartbeat));
                } // else ignore and discard heartbeat
            } else { // client with the same address had been connected
                auto &client = *client_ptr;
                if (client->session_id == heartbeat.session_id) {
                    client->heart_has_beaten(round_no);
                    client->player.turn_direction = heartbeat.turn_direction;

                    if (current_game.has_value()) {
                        current_game->respond_with_events(send_queue, sock, sender,
                                                          heartbeat.next_expected_event_no);
                    } else if (previous_game.has_value()) {
                        previous_game->respond_with_events(send_queue, sock, sender,
                                                           heartbeat.next_expected_event_no);
                    }

                    if (!current_game.has_value() &&
                        (heartbeat.turn_direction == LEFT || heartbeat.turn_direction == RIGHT)) {
                        client->player.got_ready();
                        try_start_game();
                    }

                } else if (client->session_id > heartbeat.session_id) {
                    disconnect_client(client_ptr);
                    connect_client(sender, std::move(heartbeat));
                }
            }
        } catch (BadData const&) {
            // Ignore invalid heartbeat.
            receive_buff.discard();
        }
    }

    void Server::connect_client(sockaddr_in6 const &addr, ClientHeartbeat heartbeat) {
        auto player = std::make_shared<Player>(std::move(heartbeat.player_name),
                                               heartbeat.turn_direction);

        auto [client_it, _] = connected_clients.emplace(std::make_shared<ClientData>(
                addr, heartbeat.session_id, round_no, *player));

        player->attach_to_client(*client_it);

        if (player->is_observer()) {
            connected_unnames.insert(player);
        }
        else {
            connected_players.insert(player);
            player_names.insert(player->player_name);
        }

        if (current_game.has_value()) {
            current_game->add_observer(player);
        }
    }

    void Server::disconnect_client(
            std::set<std::shared_ptr<ClientData>, ClientData::Comparator>::iterator client_ptr) {
        auto& client = *client_ptr;

        if (client->player.is_observer()) {
            auto it = connected_unnames.find(client->player);
            auto player_temp_copy = *it;
            connected_unnames.erase(it);
            player_temp_copy->disconnect();
        } else {
            player_names.erase(client->player.player_name);
            auto it = connected_players.find(client->player);
            auto player_temp_copy = *it;
            connected_players.erase(it);
            player_temp_copy->disconnect();
        }
        connected_clients.erase(client_ptr);
    }

    void Server::mainloop() {
        {
            long sec = static_cast<long>(round_duration_ns / NS_IN_SEC);
            long ns = static_cast<long>(round_duration_ns % NS_IN_SEC);
            struct timespec spec{.tv_sec = sec, .tv_nsec = ns};
            struct itimerspec conf{.it_interval = spec, .it_value = spec};
            verify(timerfd_settime(round_timer, 0, &conf, nullptr), "timerfd_settime");
        }
        struct epoll_event event{};
        for (;;) {
            event = epoll.wait();
            if (event.data.fd == round_timer) {
                disconnect_idles();
                uint64_t expirations;
                if (read(round_timer, &expirations, sizeof(expirations)) != -1) {
                    for (size_t i = 0; i < expirations; ++i) {
                        round_routine();
                    }
                }
            } else if (event.events & EPOLLOUT) {
                // drain server queue
                if (drain_queue()) // if no delay this time
                    epoll.stop_watching_fd_for_output(sock);
                // else keep us notified about socket possibility to send
            } else {
                // receive heartbeat from _client
                handle_heartbeat();
            }
        }
    }
}