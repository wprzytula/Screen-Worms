#ifndef ROBAKI_GAMECONSTANTS_H
#define ROBAKI_GAMECONSTANTS_H

#include "defs.h"

namespace Worms {
    struct GameConstants {
        uint32_t const turning_speed;
        uint32_t const round_per_sec;
        uint32_t const width;
        uint32_t const height;

        GameConstants(uint32_t const turning_speed, uint32_t const round_per_sec,
                      uint32_t const width, uint32_t const height)
                : turning_speed(turning_speed), round_per_sec(round_per_sec),
                  width(width), height(height) {}
    };
}

#endif //ROBAKI_GAMECONSTANTS_H
