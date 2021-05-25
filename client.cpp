#include <getopt.h>

#include "Client.h"

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
        }
    }

    Worms::Client client{std::move(player_name), game_server, server_port,
                  game_iface, iface_port};

    client.play();
}
