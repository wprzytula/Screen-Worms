#ifndef ROBAKI_BOARD_H
#define ROBAKI_BOARD_H

#include "defs.h"
#include "Buffer.h"
#include "Pixel.h"
#include "ClientData.h"

namespace Worms {
    struct angle_t {
        static constexpr uint16_t const MAX_ANGLE = 360u;
        uint16_t angle;

        angle_t(uint16_t angle) : angle{static_cast<uint16_t>(angle % MAX_ANGLE)} {}

        angle_t& operator+=(uint16_t operand) {
            angle = (angle + operand) % 360u;
            return *this;
        }

        angle_t& operator-=(uint16_t operand) {
            angle = (angle - operand) % 360u;
            return *this;
        }

        [[nodiscard]] uint16_t value() const {
            return angle;
        }
    };

    class Position {
    private:
        double x;
        double y;
    public:
        Position(double x, double y): x{x}, y{y} {}

        void move_with_angle(angle_t angle) {
            x += std::cos(angle.value());
            y += std::sin(angle.value());
        }

        [[nodiscard]] Pixel as_pixel() const {
            return Pixel{static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
        }
    };

    class Board {
    private:
        std::vector<std::vector<bool>> eaten;
        GameConstants const &constants;

    public:
        explicit Board(GameConstants const &constants) : constants(constants) {
            eaten.resize(constants.width);
            for (auto &vec: eaten) {
                vec.resize(constants.height);
            }
        }

        [[nodiscard]] bool contains(Pixel const position) const {
            return position.on_board(constants.width, constants.height);
        }

        [[nodiscard]] bool is_eaten(Pixel const position) const {
            assert(contains(position));
            return eaten[position.x][position.y];
        }

        void eat(Pixel const position) {
            assert(contains(position));
            assert(!eaten[position.x][position.y]);
            eaten[position.x][position.y] = true;
        }
    };
}

#endif //ROBAKI_BOARD_H
