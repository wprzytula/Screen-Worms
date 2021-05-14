#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <cassert>
#include <csignal>

#include "err.h"

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

    class Game {
    public:
        std::vector<Event> events;
    };

    class Server {
    private:
        uint16_t const port;
        int const sock;
        RandomGenerator rand;
        uint32_t const turning_speed;
        uint32_t const round_per_sec;
        uint32_t const width;
        uint32_t const height;

    public:
        explicit Server(uint16_t port, uint32_t seed, uint32_t turning_speed,
                        uint32_t rounds_per_sec, uint32_t width, uint32_t height)
                : port{port},
                  sock{socket(PF_INET6, SOCK_STREAM, 0)},
                  rand{RandomGenerator{seed}},
                  turning_speed{turning_speed},
                  round_per_sec{rounds_per_sec},
                  width{width},
                  height{height} {
            int err;
            verify(sock < 0, "opening socket");

            struct sockaddr_in server_address;

            server_address.sin_family = AF_INET6;
            server_address.sin_addr.s_addr = htobe32(INADDR_ANY);
            server_address.sin_port = htobe16(port);

            verify(bind(sock, (struct sockaddr *) &server_address,
                        sizeof(server_address)) < 0, "bind");
            verify(listen(sock, QUEUE_LENGTH) < 0, "listen");
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
            fprintf(stderr, "Usage: %s \n", //FIXME
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

    Worms::Server{port, seed, turning_speed, rounds_per_sec, width, height};

    return 0;
}