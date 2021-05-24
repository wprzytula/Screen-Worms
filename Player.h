#ifndef ROBAKI_PLAYER_H
#define ROBAKI_PLAYER_H

#include "defs.h"
#include "ClientData.h"

namespace Worms {
    class PlayerWaiting {
    public:
        struct Comparator {
            using is_transparent = void;

            bool operator()(PlayerWaiting const &p1, PlayerWaiting const &p2) const {
                return ClientData::Comparator()(p1.id, p2.id);
            }
        };

        ClientData &id;
        uint8_t mutable turn_direction;
        bool mutable ready = false;

        PlayerWaiting(ClientData &id, uint8_t turn_direction)
                : id{id}, turn_direction{turn_direction} {}
    };

    class PlayerInGame {
    public:
        struct Comparator {
            using is_transparent = void;

            bool operator()(PlayerInGame const &p1, PlayerInGame const &p2) const {
                return ClientData::Comparator()(*p1.id, *p2.id);
            }
        };

        ClientData *id;
        uint8_t turn_direction;
        Position position;
        angle_t angle;

        PlayerInGame(ClientData &id, uint8_t turn_direction, uint32_t x,
                     uint32_t y, angle_t angle)
                : id{&id}, turn_direction{turn_direction},
                  position{x, y}, angle{angle} {}
    };
}

#endif //ROBAKI_PLAYER_H
