#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <unordered_map>
#include <regex>
#include <cassert>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>
#include <csignal>
#include <sys/timerfd.h>

#include "err.h"
#include "misc.h"

#define QUEUE_LENGTH 5
#define BUFFER_SIZE 1024

namespace Worms {

    class GetAddrInfoRAII {
    private:
        struct addrinfo *addr_result{};
        sockaddr_in6 _address{};
    public:
        GetAddrInfoRAII(int sock_type, char const *name, uint16_t port) {
            int sock;
            struct addrinfo addr_hints{};
            int i, flags, sflags, err;
            size_t len;
            socklen_t rcva_len;

            assert(sock_type == SOCK_DGRAM || sock_type == SOCK_STREAM);

            // 'converting' host/port in string to struct addrinfo
            (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
            addr_hints.ai_family = AF_INET6;
            addr_hints.ai_socktype = sock_type;
            addr_hints.ai_protocol = sock_type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
            addr_hints.ai_flags = 0;
            addr_hints.ai_addrlen = 0;
            addr_hints.ai_addr = nullptr;
            addr_hints.ai_canonname = nullptr;
            addr_hints.ai_next = nullptr;
            err = getaddrinfo(name, nullptr, &addr_hints, &addr_result);
            if (err == EAI_SYSTEM) { // system error
                syserr(errno, "getaddrinfo: %s", gai_strerror(err));
            }
            else if (err != 0) { // other error (host not found, etc.)
                fatal("getaddrinfo: %s", gai_strerror(err));
            }

            _address.sin6_family = AF_INET6;
            _address.sin6_addr =
                    ((struct sockaddr_in6*)addr_result->ai_addr)->sin6_addr;
            _address.sin6_port = htobe16(port); // port from the command line
        }

        ~GetAddrInfoRAII() {
            freeaddrinfo(addr_result);
        }

        [[nodiscard]] struct sockaddr_in6 address() const {
            return _address;
        }
    };

    class Client {
    private:
        timestamp_t const ts;
        std::string const player_name;
        struct sockaddr_in6 const game_server;
        int const server_sock;
        struct sockaddr_in6 const game_iface;
        int const iface_sock;
        int const heartbeat_timer;

    public:
        Client(std::string player_name, char const *game_server, uint16_t server_port,
               char const *game_iface, uint16_t iface_port)
            : ts{std::chrono::time_point_cast<std::chrono::microseconds>(
                 std::chrono::system_clock::now())},
              player_name{std::move(player_name)},
              game_server{GetAddrInfoRAII{SOCK_DGRAM, game_server, server_port}.address()},
              server_sock{socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)},
              game_iface{GetAddrInfoRAII{SOCK_STREAM, game_iface, iface_port}.address()},
              iface_sock{socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)},
              heartbeat_timer{timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK)} {
            if (server_sock < 0 || iface_sock < 0)
                syserr(errno, "opening sockets");
            if (heartbeat_timer < 0)
                syserr(errno, "opening timer fd");
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

    Worms::Client{std::move(player_name), game_server, server_port,
                  game_iface, iface_port};

    return 0;
}
