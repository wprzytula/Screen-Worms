#include <getopt.h>
#include "cerrno"

#include "Server/Server.h"

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
            goto bad_syntax;
        } else {
            errno = 0;
            char *badchar;
            parsed_arg = strtoul(optarg, &badchar, 10);
            if (*badchar != '\0' || errno != 0 || parsed_arg > UINT32_MAX || parsed_arg == 0)
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
        }
    }
    if (optind != argc) {
        bad_syntax:
        fprintf(stderr, "Usage: %s [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }


    Worms::Server server{port, seed, {turning_speed, rounds_per_sec, width, height}};

    server.mainloop();
}