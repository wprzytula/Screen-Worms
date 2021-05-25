#ifndef ROBAKI_SERVER_H
#define ROBAKI_SERVER_H

#include <fcntl.h>

#include <queue>

#include <iostream>
using std::cout;

#include "defs.h"
#include "Buffer.h"
#include "RandomGenerator.h"
#include "GameConstants.h"
#include "Epoll.h"
#include "ClientHeartbeat.h"
#include "Player.h"
#include "Game.h"

namespace Worms {
    class Server {
    private:
        static constexpr uint64_t const NS_IN_SEC = 1'000'000'000;
        static constexpr uint64_t const DISCONNECT_THRESHOLD = 2 * NS_IN_SEC;

        int const sock;
        int const round_timer;
        Epoll epoll;
        uint64_t round_no = 0;
        RandomGenerator rand;
        GameConstants const constants;
        uint64_t const round_duration_ns;
        std::optional<Game> current_game;
        std::optional<Game> previous_game;
        std::queue<UDPSendBuffer> send_queue;
        UDPReceiveBuffer receive_buff;

        std::set<std::shared_ptr<ClientData>, ClientData::Comparator> connected_clients;
        std::set<std::shared_ptr<Player>, Player::Comparator> connected_players;
        std::set<std::shared_ptr<Player>, Player::Comparator> connected_unnames;
        std::set<std::string> player_names;

    public:
        explicit Server(uint16_t const port, uint32_t const seed, GameConstants constants)
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
            server_address.sin6_port = htobe(port);

            verify(bind(sock, (struct sockaddr *) &server_address,
                        sizeof(server_address)), "bind");

            verify(fcntl(sock, F_SETFL, O_NONBLOCK), "fcntl");

            epoll.add_fd(sock);
            epoll.watch_fd_for_input(round_timer);
            epoll.watch_fd_for_input(sock);
        }
        ~Server() {
            if (close(sock) != 0)
                fputs("Error closing server_addr socket", stderr);
            if (close(round_timer) != 0)
                fputs("Error closing timer fd", stderr);
        }
    private:
        void disconnect_idles() {
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

        void round_routine() {
            if (current_game.has_value()) {
                current_game->play_round();
                current_game->disseminate_new_events(send_queue, sock);
                if (current_game->finished()) {
                    previous_game.emplace(std::move(current_game.value()));
                    current_game.reset();
                }
            } else { // Check if a game can be started.
                if (connected_players.size() >= 2) {
                    bool all_ready = true;
                    for (auto& player: connected_players) {
                        if (!player->is_ready()) {
                            all_ready = false;
                            break;
                        }
                    }
                    if (all_ready)
                        start_game();
                }
            }

            ++round_no;

            if (!drain_queue())
                epoll.watch_fd_for_output(sock);
        }

        void start_game() {
            std::vector<std::weak_ptr<Player>> observers;
            for (auto& observer: connected_unnames) {
                observers.push_back(std::weak_ptr<Player>{observer});
            }
            current_game.emplace(constants, rand, connected_players, std::move(observers));
        }

        bool drain_queue() {
            while (!send_queue.empty()) {
                if (send_queue.front().flush())
                    send_queue.pop();
                else
                    return false;
            }
            return true;
        }

        void handle_heartbeat() {
            auto sender = receive_buff.populate();
            ClientHeartbeat heartbeat{receive_buff};

            auto client_ptr = connected_clients.find(sender);
            if (connected_clients.find(sender) == connected_clients.end()) {
//                cout << "Address not found so far.\n";
                if (player_names.find(heartbeat.player_name) == player_names.end()) {
                    connect_client(sender, std::move(heartbeat));
                } // else ignore and discard heartbeat
                else {
//                    cout << "Name already taken.\n";
                }
            } else { // client with the same address had been connected
//                cout << "Address already taken\n";
                auto &client = *client_ptr;
                if (client->session_id == heartbeat.session_id) {
                    client->heart_has_beaten(round_no);
                    client->player.turn_direction = heartbeat.turn_direction;
                    if (heartbeat.turn_direction == LEFT || heartbeat.turn_direction == RIGHT)
                        client->player.got_ready();

                    if (current_game.has_value()) {
                        current_game->respond_with_events(send_queue, sock, sender,
                                                          heartbeat.next_expected_event_no);
                    }
                } else if (client->session_id > heartbeat.session_id) {
                    disconnect_client(client_ptr);
                    connect_client(sender, std::move(heartbeat));
                }
            }
        }

        void connect_client(sockaddr_in6 const& addr, ClientHeartbeat heartbeat) {
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

            cout << "Connected client: " <<
                (player->player_name.empty() ? "(empty)" : player->player_name)
                << '\n';
        }

        void disconnect_client(decltype(connected_clients.begin()) client_ptr) {
            auto& client = *client_ptr;
            cout << "Disconnected client: " <<
                (client->player.player_name.empty() ? "(empty)" : client->player.player_name)
                << '\n';

            if (client->player.is_observer()) {
                connected_unnames.erase(connected_unnames.find(client->player));
            } else {
                player_names.erase(client->player.player_name);
                connected_players.erase(connected_players.find(client->player));
            }
            client->player.disconnect();
            connected_clients.erase(client_ptr);
        }

    public:
        [[noreturn]] void mainloop() {
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
//                    verify(read(round_timer, &expirations, sizeof(expirations)),
//                           "read timerfd");
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
    };
}

#endif //ROBAKI_SERVER_H
