#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <cassert>
#include <csignal>
#include <memory>
#include <ctgmath>

#include "err.h"
#include "event.h"
#include "utils.h"

#define QUEUE_LENGTH 5
#define BUFFER_SIZE 1024

namespace Worms {
    class RandomGenerator {
    private:
        uint32_t next_val;
    public:
        explicit RandomGenerator(uint32_t seed) : next_val{seed} {}

        uint32_t operator()() {
            uint32_t const ret = next_val;
            next_val = (uint64_t{next_val} * 279410273ULL) % 4294967291;
            return ret;
        }
    };

    struct angle_t {
        static constexpr uint16_t const MAX_ANGLE = 360u;
        uint16_t angle;
        angle_t(uint16_t angle) : angle{static_cast<uint16_t>(angle % MAX_ANGLE)} {}
        angle_t& operator+=(uint16_t operand) {
            angle = (angle + operand) % 360u;
            return *this;
        }
        [[nodiscard]] uint16_t value() const {
            return angle;
        }
    };

    class Position {
    private:
        double x;
        double y;
    public:
        Position(double x, double y) : x{x}, y{y} {}
        void move_with_angle(angle_t angle) {
            x += std::cos(angle.value());
            y += std::sin(angle.value());
        }
        [[nodiscard]] Pixel as_pixel() const {
            return Pixel{static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
        }
    };


    struct GameConstants {
        uint32_t const turning_speed;
        uint32_t const round_per_sec;
        uint32_t const width;
        uint32_t const height;

        GameConstants(uint32_t const turning_speed, uint32_t const round_per_sec,
                      uint32_t const width, uint32_t const height)
                : turning_speed(turning_speed), round_per_sec(round_per_sec),
                  width(width), height(height) {}
    };

    using PlayerID = uint64_t;

    class Player {
    private:

    };

    class Board {
    private:
        class Field {
        private:
            bool is_eaten;
            std::optional<PlayerID> worm_here;
        public:
            explicit Field() : is_eaten(false) {}
            [[nodiscard]] bool will_collide(PlayerID const playerId) const {
                return is_eaten &&
                       (!worm_here.has_value() || worm_here.value() != playerId);
            }
            void put_player(PlayerID const playerId) {
                assert((!is_eaten && !worm_here.has_value()) || worm_here.value() == playerId);
                worm_here = playerId;
                is_eaten = true;
            }
        };
        using board_t = std::vector<std::vector<Field>>;
        board_t fields;
        GameConstants const& constants;

    public:
        explicit Board(GameConstants const &constants) : constants(constants) {
            fields.resize(constants.width);
            for (auto& vec: fields) {
                vec.resize(constants.height);
            }
        }

        [[nodiscard]] bool contains(Pixel const position) const {
            return position.on_board(constants.width, constants.height);
        }

        [[nodiscard]] bool will_collide(PlayerID const playerId, Pixel const position) const {
            assert(contains(position));
            return fields[position.x][position.y].will_collide(playerId);
        }

        void put_player(PlayerID const playerId, Pixel const position) {
            assert(contains(position));
            fields[position.x][position.y].put_player(playerId);
        }
    };

    class Game {

    private:
        Board board;
        std::vector<std::shared_ptr<Event const>> events;
    public:
        explicit Game(GameConstants const& constants) : board{constants} {}


    };

    class Server {
    private:
        uint16_t const port;
        int const sock;
        RandomGenerator rand;
        GameConstants const constants;
        std::optional<Game> current_game;
        std::unordered_map<std::string, std::string> player_addr;

    public:
        explicit Server(uint16_t const port, uint32_t const seed, GameConstants constants)
                : port{port},
                  sock{socket(PF_INET6, SOCK_STREAM, 0)},
                  rand{RandomGenerator{seed}},
                  constants{constants} {
            int err;
            verify(sock < 0, "opening socket");

            struct sockaddr_in6 server_address{};

            server_address.sin6_family = AF_INET6;
            server_address.sin6_addr = in6addr_any;
            server_address.sin6_port = htobe16(port);

            verify(bind(sock, (struct sockaddr *) &server_address,
                        sizeof(server_address)) < 0, "bind");
        }
        ~Server() {
            if (close(sock) != 0)
                fputs("closing server socket", stderr);
        }
        void run() {

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
            fprintf(stderr, "Usage: %s [-p port] [-s seed] [-t turning_speed] [-v rounds_per_sec] [-w width] [-h height]\n", //FIXME
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



    return 0;
}