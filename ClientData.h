#ifndef ROBAKI_CLIENTDATA_H
#define ROBAKI_CLIENTDATA_H

#include "defs.h"

namespace Worms {
    class Player;

    struct ClientData {
        struct Comparator {
            using is_transparent = void;

            bool operator()(ClientData const &c1, ClientData const &c2) const {
                return memcmp(&c1.address, &c2.address, sizeof(c1.address)) < 0;
            }

            bool operator()(sockaddr_in6 const &addr, ClientData const& c) const {
                return memcmp(&addr, &c.address, sizeof(c.address)) < 0;
            }

            bool operator()(ClientData const& c, sockaddr_in6 const &addr) const {
                return memcmp(&addr, &c.address, sizeof(c.address)) < 0;
            }
        };
        struct PtrComparator {
            using is_transparent = void;

            bool operator()(std::shared_ptr<ClientData> const &c1, std::shared_ptr<ClientData> const &c2) const {
                return memcmp(&c1->address, &c2->address, sizeof(c1->address)) < 0;
            }

            bool operator()(sockaddr_in6 const &addr, std::shared_ptr<ClientData> const& c) const {
                return memcmp(&addr, &c->address, sizeof(c->address)) < 0;
            }

            bool operator()(std::shared_ptr<ClientData> const& c, sockaddr_in6 const &addr) const {
                return memcmp(&c->address, &addr, sizeof(c->address)) < 0;
            }
        };


        sockaddr_in6 const address;
        uint64_t const session_id;
        uint64_t mutable last_heartbeat_round_no;
        Player& player;

        ClientData(sockaddr_in6 const &address, uint64_t const session_id,
                   uint64_t last_heartbeat_round_no, Player &player)
                   : address{address}, session_id{session_id},
                     last_heartbeat_round_no{last_heartbeat_round_no}, player{player} {}

        void heart_has_beaten(uint64_t round_no) {
            last_heartbeat_round_no = round_no;
        }
        /*bool operator==(ClientData const &p2) const {
            Comparator cmp;
            return !cmp(*this, p2) && !cmp(p2, *this);
        }

        bool operator!=(ClientData const &p2) const {
            return !(*this == p2);
        }*/
    };
}

#endif //ROBAKI_CLIENTDATA_H
