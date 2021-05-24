#include <arpa/inet.h>
#include "defs.h"

namespace Worms {
    int gai_sock_factory(int sock_type, char const *name, uint16_t port) {
        struct addrinfo *addr_result{};
        struct sockaddr_in6 _address{};
        struct addrinfo addr_hints{};
        int err;

        assert(sock_type == SOCK_DGRAM || sock_type == SOCK_STREAM);

        // 'converting' host/port in string to struct addrinfo
        memset(&addr_hints, 0, sizeof(struct addrinfo));
        addr_hints.ai_family = AF_UNSPEC;
        addr_hints.ai_socktype = sock_type;
        addr_hints.ai_protocol = sock_type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;

        err = getaddrinfo(name, std::to_string(port).c_str(), &addr_hints, &addr_result);
        if (err == EAI_SYSTEM) { // system error
            syserr(errno, "getaddrinfo: %s", gai_strerror(err));
        } else if (err != 0) { // other error (host not found, etc.)
            fatal("getaddrinfo: %s", gai_strerror(err));
        } else {
            char ip[100];
            for (auto ptr = addr_result; ptr != nullptr; ptr = ptr->ai_next) {
                int sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                verify(sock, "opening socket");

                if (connect(sock, ptr->ai_addr, ptr->ai_addrlen) == 0) {
                    freeaddrinfo(addr_result);
                    return sock;
                } else {
                    close(sock);
                }
            }
        }
        freeaddrinfo(addr_result);
        syserr(errno, "connect to iface");
        return -1;
    }
}
