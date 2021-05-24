#ifndef ROBAKI_PLAYER_H
#define ROBAKI_PLAYER_H

#include "defs.h"
#include "ClientData.h"

namespace Worms {
    class Player {
    public:
        struct Comparator {
            using is_transparent = void;

            bool operator()(Player const &p1, Player const &p2) const {
                return ClientData::Comparator()(*p1.client, *p2.client);
            }
        };
        std::shared_ptr<ClientData> client;
        std::string const& player_name;
        bool mutable ready = false;
        uint8_t mutable turn_direction;
        std::optional<Position> position;
        angle_t angle;

        Player(ClientData &id, std::string const& player_name, uint8_t turn_direction,
               uint32_t x, uint32_t y, angle_t angle)
                : player_name{player_name}, turn_direction{turn_direction},
                  /*position{x, y}, */angle{angle} {
        }
    };
}

#endif //ROBAKI_PLAYER_H
