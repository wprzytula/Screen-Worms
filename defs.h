#ifndef ROBAKI_DEFS_H
#define ROBAKI_DEFS_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cassert>
#include <cstring>

#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "err.h"

namespace Worms {
    int gai_sock_factory(int sock_type, char const *name, uint16_t port);

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    inline T htobe(T field) {
        constexpr size_t size = sizeof(field);
        if constexpr(size == 1) {
            return field;
        } else if constexpr (size == 2) {
            return htobe16(field);
        } else if constexpr (size == 4) {
            return htobe32(field);
        } else if constexpr (size == 8) {
            return htobe64(field);
        } else {
            assert(false);
        }
    }

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    inline T betoh(T field) {
        constexpr size_t size = sizeof(field);
        if constexpr(size == 1) {
            return field;
        } else if constexpr (size == 2) {
            return be16toh(field);
        } else if constexpr (size == 4) {
            return be32toh(field);
        } else if constexpr (size == 8) {
            return be64toh(field);
        } else {
            assert(false);
        }
    }
}

#endif //ROBAKI_DEFS_H
