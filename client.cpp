#include <fcntl.h>
#include <sys/timerfd.h>
#include <getopt.h>

#include "defs.h"
#include "GetAddrInfoRAII.h"

#define QUEUE_LENGTH 5
#define BUFFER_SIZE 1024

namespace Worms {

    class Client {
    private:
        timestamp_t const ts;
        std::string const player_name;
        struct sockaddr_in6 const server_addr;
        int const server_sock;
        struct sockaddr_in6 const iface_addr;
        int const iface_sock;
        int const heartbeat_timer;

    public:
        Client(std::string player_name, char const *game_server, uint16_t server_port,
               char const *game_iface, uint16_t iface_port)
            : ts{std::chrono::time_point_cast<std::chrono::microseconds>(
                 std::chrono::system_clock::now())},
              player_name{std::move(player_name)},
              server_addr{GetAddrInfoRAII{SOCK_DGRAM, game_server, server_port}.address()},
              server_sock{socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)},
              iface_addr{GetAddrInfoRAII{SOCK_STREAM, game_iface, iface_port}.address()},
              iface_sock{socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)},
              heartbeat_timer{timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK)} {
            if (server_sock < 0 || iface_sock < 0)
                syserr(errno, "opening sockets");
            if (heartbeat_timer < 0)
                syserr(errno, "opening timer fd");
            verify(connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)),
                    "connecting to server");
            verify(connect(iface_sock, (struct sockaddr*)&iface_addr, sizeof(iface_addr)),
                   "connecting to interface");
        }
        ~Client() {
            if (close(server_sock) != 0)
                fputs("Error closing server socket", stderr);
            if (close(iface_sock) != 0)
                fputs("Error closing interface socket", stderr);
            if (close(heartbeat_timer) != 0)
                fputs("Error closing timer fd", stderr);
        }
    };
}

int main(int argc, char *argv[]) {
    int opt;
    char const *game_server;
    char const *game_iface = "localhost";
    std::string player_name;
    uint16_t server_port = 2021;
    uint16_t iface_port = 20210;
    unsigned long parsed_arg;

    if (argc < 2) {
    bad_syntax:
        fprintf(stderr, "Usage: %s game_server [-n player_name]"
                        " [-p n] [-i gui_server] [-r n]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    game_server = argv[1];

    while ((opt = getopt(argc, argv, "n:p:i:r:")) != -1) {
        if (opt == '?') {
            goto bad_syntax;
        } else {
            switch (opt) {
                case 'p':
                case 'r':
                    parsed_arg = strtoul(optarg, nullptr, 10);
                    if (errno != 0 || parsed_arg > UINT16_MAX)
                        goto bad_syntax;
                    if (opt == 'p')
                        server_port = parsed_arg;
                    else
                        iface_port = parsed_arg;
                    break;
                case 'n':
                    player_name = optarg;
                    break;
                case 'i':
                    game_iface = optarg;
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

    Worms::Client client{std::move(player_name), game_server, server_port,
                  game_iface, iface_port};

    return 0;
}
