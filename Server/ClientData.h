#ifndef ROBAKI_CLIENTDATA_H
#define ROBAKI_CLIENTDATA_H

#include <cstring>
#include <netinet/in.h>

namespace Worms {
    class Player;

    struct ClientData {
        struct Comparator {
            using is_transparent = void;

            bool operator()(sockaddr_in6 const& addr1, sockaddr_in6 const& addr2) const {
                return memcmp(&addr1, &addr2, sizeof(sockaddr_in6)) < 0;
            }

            bool operator()(ClientData const &c1, ClientData const &c2) const {
                return operator()(c1.address, c2.address);
            }

            bool operator()(sockaddr_in6 const &addr, ClientData const& c) const {
                return operator()(addr, c.address);
            }

            bool operator()(ClientData const& c, sockaddr_in6 const &addr) const {
                return operator()(c.address, addr);
            }

            bool operator()(std::shared_ptr<ClientData> const &c1,
                    std::shared_ptr<ClientData> const &c2) const {
                return operator()(*c1, *c2);
            }

            bool operator()(sockaddr_in6 const &addr, std::shared_ptr<ClientData> const& c) const {
                return operator()(addr, c->address);
            }

            bool operator()(std::shared_ptr<ClientData> const& c, sockaddr_in6 const &addr) const {
                return operator()(c->address, addr);
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
    };
}

#endif //ROBAKI_CLIENTDATA_H
