#include <fcntl.h>
#include <sys/timerfd.h>
#include <getopt.h>
#include <netinet/tcp.h>

#include "defs.h"
#include "GetAddrInfoRAII.h"
#include "Epoll.h"
#include "Buffer.h"
#include "ClientHeartbeat.h"

#define QUEUE_LENGTH 5
#define BUFFER_SIZE 1024

namespace Worms {

    class Client {
    private:
        static constexpr long const COMMUNICATION_INTERVAL = 30'000'000;
        static constexpr long const INITIAL_IFACE_BUFF_CAP = 256;

        uint64_t const session_id;
        std::string const player_name;
        struct sockaddr_in6 const server_addr;
        int const server_sock;
        struct sockaddr_in6 const iface_addr;
        int const iface_sock;
        int const heartbeat_timer;

        Epoll epoll;
        UDPSendBuffer server_send_buff;
        TCPSendBuffer iface_send_buff;
        uint8_t turn_direction;
        uint32_t next_expected_event_no;
        bool queued_heartbeat;


    public:
        Client(std::string player_name, char const *game_server, uint16_t server_port,
               char const *game_iface, uint16_t iface_port)
            : session_id{static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count())},
              player_name{std::move(player_name)},
              server_addr{GetAddrInfoRAII{SOCK_DGRAM, game_server, server_port}.address()},
              server_sock{socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP)},
              iface_addr{GetAddrInfoRAII{SOCK_STREAM, game_iface, iface_port}.address()},
              iface_sock{socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)},
              heartbeat_timer{timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK)},
              epoll{heartbeat_timer},
              server_send_buff{UDPEndpoint{server_sock, server_addr, sizeof(server_addr)}},
              iface_send_buff{iface_sock, INITIAL_IFACE_BUFF_CAP},
              turn_direction{0}, next_expected_event_no{0}, queued_heartbeat{false} {

            if (server_sock < 0 || iface_sock < 0)
                syserr(errno, "opening sockets");
            if (heartbeat_timer < 0)
                syserr(errno, "opening timer fd");

            int optval = 1;
            verify(setsockopt(iface_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)),
                   "setsockopt");

            verify(connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)),
                    "connecting to server");
            verify(connect(iface_sock, (struct sockaddr*)&iface_addr, sizeof(iface_addr)),
                   "connecting to interface");

            epoll.add_fd(server_sock);
            epoll.add_fd(iface_sock);
            epoll.watch_fd_for_input(heartbeat_timer);
            epoll.watch_fd_for_input(server_sock);
            epoll.watch_fd_for_input(iface_sock);
        }

        ~Client() {
            if (close(server_sock) != 0)
                fputs("Error closing server socket", stderr);
            if (close(iface_sock) != 0)
                fputs("Error closing interface socket", stderr);
            if (close(heartbeat_timer) != 0)
                fputs("Error closing timer fd", stderr);
        }

        void handle_iface_msg() {

        }

        void send_heartbeat() {
            server_send_buff.clear();
            ClientHeartbeat heartbeat{session_id, turn_direction, next_expected_event_no,
                                      player_name};
            heartbeat.pack(server_send_buff);
            if (server_send_buff.flush() == -1) {
                queued_heartbeat = true;
                epoll.watch_fd_for_output(server_sock);
            }
        }

        void drain_server_queue() {
            assert(queued_heartbeat);
            queued_heartbeat = false;
            epoll.stop_watching_fd_for_output(server_sock);
            assert(server_send_buff.flush() == server_send_buff.size());
        }

        [[noreturn]] void play() {
            {
                struct timespec spec{.tv_sec = 0, .tv_nsec = COMMUNICATION_INTERVAL};
                struct itimerspec conf{.it_interval = spec, .it_value = spec};
                timerfd_settime(heartbeat_timer, 0, &conf, nullptr);
            }
            struct epoll_event event{};
            for (;;) {
                event = epoll.wait();
                if (event.data.fd == heartbeat_timer) {
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
                    } else {
                        // receive pressed/released from interface
                        handle_iface_msg();
                    }
                }
            }
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
