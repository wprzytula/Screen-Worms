#include <cstdint>
#include <netdb.h>
#include <cstring>
#include <cassert>
#include <unistd.h>

#include <string>

#include "../Common/err.h"


namespace Worms {
    /* Attempts to resolve given hostname or address using getaddrinfo, then creates
     * a socket of appriopriate family and connects to resolved address using it.
     * If succeeded, returns the socket, else raises errors. */
    int gai_sock_factory(int sock_type, char const *name, uint16_t port) {
        struct addrinfo *addr_result{};
        struct addrinfo addr_hints{};
        int err;

        assert(sock_type == SOCK_DGRAM || sock_type == SOCK_STREAM);

        memset(&addr_hints, 0, sizeof(struct addrinfo));
        addr_hints.ai_family = AF_UNSPEC;
        addr_hints.ai_socktype = sock_type;
        addr_hints.ai_protocol = sock_type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;

        err = getaddrinfo(name, std::to_string(port).c_str(), &addr_hints, &addr_result);
        if (err == EAI_SYSTEM) {
            syserr(errno, "getaddrinfo: %s", gai_strerror(err));
        } else if (err != 0) {
            fatal("getaddrinfo: %s", gai_strerror(err));
        } else {
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
