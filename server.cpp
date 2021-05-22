#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <cassert>
#include <memory>
#include <ctgmath>
#include <sys/fcntl.h>
#include <queue>
#include <set>
#include <cstring>

#include "err.h"
#include "Event.h"
#include "Buffer.h"
#include "RandomGenerator.h"
#include "GameConstants.h"
#include "Board.h"

namespace Worms {
    class Player {
    private:
        PlayerID id;
        uint8_t turn_direction;
        uint32_t last_heartbeat_no;
    };

    class Game {

    private:
        Board board;
        std::vector<std::unique_ptr<Event const>> events;
        std::vector<Player> players;
        std::set<PlayerID> connected;
        uint32_t round_no;

    public:
        explicit Game(GameConstants const& constants)
            : board{constants}, round_no{0} {}
    };

    class Server {
    private:
        uint16_t const port;
        int const sock;
        RandomGenerator rand;
        GameConstants const constants;
        std::optional<Game> current_game;
        std::queue<SendBuffer> send_queue;
        std::set<PlayerID, PlayerID::Comparator> connected_players;
        int const round_timer;
    public:
        explicit Server(uint16_t const port, uint32_t const seed, GameConstants constants)
                : port{port},
                  sock{socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)},
                  rand{RandomGenerator{seed}},
                  constants{constants},
                  round_timer{timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK)} {
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

            verify(fcntl(sock, F_SETFL, O_NONBLOCK), "fcntl set");
        }
        ~Server() {
            if (close(sock) != 0)
                fputs("Error closing server socket", stderr);
            if (close(round_timer) != 0)
                fputs("Error closing timer fd", stderr);
        }
        void mainloop() {

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