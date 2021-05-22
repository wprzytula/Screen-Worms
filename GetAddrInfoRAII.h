#ifndef ROBAKI_GETADDRINFORAII_H
#define ROBAKI_GETADDRINFORAII_H

#include "defs.h"

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
            } else if (err != 0) { // other error (host not found, etc.)
                fatal("getaddrinfo: %s", gai_strerror(err));
            }

            _address.sin6_family = AF_INET6;
            _address.sin6_addr =
                    ((struct sockaddr_in6 *) addr_result->ai_addr)->sin6_addr;
            _address.sin6_port = htobe16(port); // port from the command line
        }

        ~GetAddrInfoRAII() {
            freeaddrinfo(addr_result);
        }

        [[nodiscard]] struct sockaddr_in6 address() const {
            return _address;
        }
    };
}

#endif //ROBAKI_GETADDRINFORAII_H
