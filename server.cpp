#include <cstdlib>
#include <memory>
#include <ctgmath>
#include <sys/fcntl.h>
#include <queue>
#include <set>
#include <utility>
#include <algorithm>

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
        GameConstants constants;
        Board board;
        RandomGenerator& rand;
        uint32_t const game_id;
        std::vector<std::unique_ptr<Event const>> events;
        std::vector<std::shared_ptr<Player>> players;
        std::set<std::shared_ptr<ClientData>> observers;
    public:
        Game(GameConstants constants, RandomGenerator& rand, std::set<std::shared_ptr<Player>> const& ready_players,
             std::set<std::shared_ptr<ClientData>> observers)
            : constants{constants}, board{constants}, rand{rand}, game_id{rand()},
              observers{std::move(observers)} {
            for (auto& player: ready_players) {
                players.push_back(player);
            }
            std::sort(players.begin(), players.end(),
                      [](std::shared_ptr<Player> const& p1,
                         std::shared_ptr<Player> const& p2){
                return p1->player_name < p2->player_name;
            });

            {   // generate NEW_GAME
                std::vector<std::string> player_names;
                player_names.resize(players.size());
                for (size_t i = 0; i < players.size(); ++i) {
                    player_names[i] = players[i]->player_name;
                }
                generate_event(NEW_GAME_NUM, std::make_unique<Data_NEW_GAME>(
                        constants.width, constants.height, std::move(player_names)));
            }
            for (size_t i = 0; i < players.size(); ++i) {
                auto& player = players[i];
                player->position.emplace(rand() % constants.width + 0.5,
                                         rand() % constants.height + 0.5);
                player->angle = rand() % 360;
                auto player_pixel = player->position->as_pixel();
                if (board.is_eaten(player_pixel)) {
                    generate_event(PLAYER_ELIMINATED_NUM,
                                   std::make_unique<Data_PLAYER_ELIMINATED>(i));
                } else {
                    board.eat(player_pixel);
                    generate_event(PIXEL_NUM,std::make_unique<Data_PIXEL>(
                            i, player_pixel.x, player_pixel.y));
                }
            }
        }

        void generate_event(uint8_t event_type, std::unique_ptr<EventDataIface> data) {
            uint32_t event_no = events.size();
            std::unique_ptr<Event> event;

            switch (event_type) {
                case NEW_GAME_NUM: {
                    event = std::make_unique<Event_NEW_GAME>(
                            event_no, event_type, *(dynamic_cast<Data_NEW_GAME*>(data.get())));
                    break;
                }
                case PIXEL_NUM: {
                    event = std::make_unique<Event_PIXEL>(
                            event_no, event_type, *(dynamic_cast<Data_PIXEL*>(data.get())));
                    break;
                }
                case PLAYER_ELIMINATED_NUM:
                    event = std::make_unique<Event_PLAYER_ELIMINATED>(
                            event_no, event_type, *(dynamic_cast<Data_PLAYER_ELIMINATED*>(data.get())));
                    break;
                case GAME_OVER_NUM:
                    event = std::make_unique<Event_GAME_OVER>(
                            event_no, event_type, *(dynamic_cast<Data_GAME_OVER*>(data.get())));
                    break;
                default:
                    assert(false);
            }
            events.push_back(std::move(event));
        }
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
//                it->
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