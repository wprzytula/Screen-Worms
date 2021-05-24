#include <cstdlib>
#include <memory>
#include <ctgmath>
#include <sys/fcntl.h>
#include <queue>
#include <set>
#include <utility>

#include "err.h"
#include "Event.h"
#include "Buffer.h"
#include "RandomGenerator.h"
#include "GameConstants.h"
#include "Board.h"
#include "Epoll.h"
#include "ClientHeartbeat.h"
#include "Player.h"

namespace Worms {
    class Game {
    private:
        Board board;
        std::vector<std::unique_ptr<Event const>> events;
        std::vector<PlayerInGame> players;
//        std::set<ClientData> connected;
        uint32_t round_no;

    public:
        explicit Game(GameConstants const& constants) : board{constants}, round_no{0} {}
    };

    class Server {
    private:
        uint16_t const port;
        int const sock;
        int const round_timer;
        Epoll epoll;
        uint64_t round_no;
        RandomGenerator rand;
        GameConstants const constants;
        uint64_t const round_duration_ns;
        std::optional<Game> current_game;
        std::queue<UDPSendBuffer> send_queue;
        UDPReceiveBuffer receive_buff{sock};

        std::set<ClientData, ClientData::Comparator> connected_players;
        std::set<std::string> player_names;
    public:
        explicit Server(uint16_t const port, uint32_t const seed, GameConstants constants)
                : port{port},
                  sock{socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)},
                  round_timer{timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK)},
                  epoll{round_timer},
                  round_no{0},
                  rand{RandomGenerator{seed}},
                  constants{constants},
                  round_duration_ns{1'000'000'000 / constants.round_per_sec}{
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
        }
        ~Server() {
            if (close(sock) != 0)
                fputs("Error closing server_addr socket", stderr);
            if (close(round_timer) != 0)
                fputs("Error closing timer fd", stderr);
        }
    private:
        void disconnect_idles() {
            std::vector<typeof(connected_players.begin())> to_disconnect;
            for (auto it = connected_players.begin(); it != connected_players.end(); ++it) {
                if ((round_no - it->last_heartbeat_round_no) * round_duration_ns >= 2'000'000'000) {
                    to_disconnect.push_back(it);
                }
            }
            for (auto it : to_disconnect) {
                connected_players.erase(it);
            }
        }

        void play_round() {

        }

        bool drain_queue() {
            assert(!send_queue.empty());
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

            if (connected_players.find(sender) == connected_players.end()) {
                if (player_names.find(heartbeat.player_name) == player_names.end()) {
                    connect_client();
                } else {
                    // discard, ignore, annihilate!
                }
            } else {
                auto const& client = connected_players.find(sender);
                if (client->session_id < heartbeat.session_id) {
                    // discard, ignore, annihilate!
                } else if (client->session_id > heartbeat.session_id) {

                } else {
                    client->last_heartbeat_round_no = round_no;
                }
            }

        }

        void connect_client() {

        }

    public:
        [[noreturn]] void mainloop() {
            {
                struct timespec spec{.tv_sec = 0, .tv_nsec = static_cast<long>(round_duration_ns)};
                struct itimerspec conf{.it_interval = spec, .it_value = spec};
                timerfd_settime(round_timer, 0, &conf, nullptr);
            }
            struct epoll_event event{};
            for (;;) {
                event = epoll.wait();
                if (event.data.fd == round_timer) {
                    disconnect_idles();
                    uint64_t expirations;
                    verify(read(round_timer, &expirations, sizeof(expirations)),
                           "read timerfd");
                    for (size_t i = 0; i < expirations; ++i) {
                        play_round();
                    }
                } else if (event.events & EPOLLOUT) {
                    // drain server queue
                    if (drain_queue()) // if no delay this time
                        epoll.stop_watching_fd_for_output(sock);
                    // else keep us notified about socket possibility to send
                } else {
                    // receive heartbeat from client
                    handle_heartbeat();
                }
            }
        }
    };
}

int main(int argc, char *argv[]) {
    int opt;
    uint16_t port = 2021;
    uint32_t seed = time(nullptr);
    uint32_t turning_speed = 6;
    uint32_t rounds_per_sec = 50;
    uint32_t width = 640;
    uint32_t height = 480;
    unsigned long parsed_arg;

    while ((opt = getopt(argc, argv, "p:s:t:v:w:h:")) != -1) {
        if (opt == '?') {
            bad_syntax:
            fprintf(stderr, "Usage: %s [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        } else {
            errno = 0;
            parsed_arg = strtoul(optarg, nullptr, 10);
            if (errno != 0 || parsed_arg > UINT32_MAX)
                goto bad_syntax;

            switch (opt) {
                case 'p':
                    if (parsed_arg > UINT16_MAX)
                        goto bad_syntax;
                    port = parsed_arg;
                    break;
                case 's':
                    seed = parsed_arg;
                    break;
                case 't':
                    turning_speed = parsed_arg;
                    break;
                case 'v':
                    rounds_per_sec = parsed_arg;
                    break;
                case 'w':
                    width = parsed_arg;
                    break;
                case 'h':
                    height = parsed_arg;
                    break;
                default:
                    goto bad_syntax;
            }

//            printf("flags=%d; tfnd=%d; optind=%d\n", flags, tfnd, optind);

            if (optind >= argc) {
                fprintf(stderr, "Expected argument after options\n");
                exit(EXIT_FAILURE);
            }

            printf("name argument = %s\n", argv[optind]);
        }
    }

    Worms::Server server{port, seed, {turning_speed, rounds_per_sec, width, height}};

    server.mainloop();

    return 0;
}