#ifndef ROBAKI_PLAYERID_H
#define ROBAKI_PLAYERID_H

#include "defs.h"

namespace Worms {
    struct PlayerID {
        struct Comparator {
            using is_transparent = void;

            bool operator()(PlayerID const &p1, PlayerID const &p2) const {
                if (p1.address.sin6_port != p2.address.sin6_port)
                    return p1.address.sin6_port < p2.address.sin6_port;
                else if (p1.addr_len != p2.addr_len)
                    return p1.addr_len < p2.addr_len;
                else
                    return memcmp(p1.address.sin6_addr.s6_addr,
                                  p2.address.sin6_addr.s6_addr, p1.addr_len);
            }
        };

        sockaddr_in6 const address;
        socklen_t const addr_len;
        uint64_t session_id;

        bool operator==(PlayerID const &p2) const {
            Comparator cmp;
            return !cmp(*this, p2) && !cmp(p2, *this);
        }

        bool operator!=(PlayerID const &p2) const {
            return !(*this == p2);
        }
    };
}

#endif //ROBAKI_PLAYERID_H
