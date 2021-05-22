#ifndef ROBAKI_PIXEL_H
#define ROBAKI_PIXEL_H

#include <cstdint>

namespace Worms {
    struct Pixel {
        uint32_t const x;
        uint32_t const y;

        [[nodiscard]] bool on_board(uint32_t board_width, uint32_t board_height) const {
            return x < board_width && y < board_height;
        }

        Pixel(uint32_t const x, uint32_t const y) : x(x), y(y) {}
    };
}

#endif //ROBAKI_PIXEL_H
