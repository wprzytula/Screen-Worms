#ifndef ROBAKI_BOARD_H
#define ROBAKI_BOARD_H

#include <cstdint>
#include "Buffer.h"
#include "Pixel.h"
#include "PlayerID.h"

namespace Worms {
    struct angle_t {
        static constexpr uint16_t const MAX_ANGLE = 360u;
        uint16_t angle;

        angle_t(uint16_t angle) : angle{static_cast<uint16_t>(angle % MAX_ANGLE)} {}

        angle_t &operator+=(uint16_t operand) {
            angle = (angle + operand) % 360u;
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
        Position(double x, double y) : x{x}, y{y} {}

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
        class Field {
        private:
            bool is_eaten;
            std::optional<PlayerID> worm_here;
        public:
            explicit Field() : is_eaten(false) {}

            [[nodiscard]] bool will_collide(PlayerID const playerId) const {
                return is_eaten &&
                       (!worm_here.has_value() || worm_here.value() != playerId);
            }

            void put_player(PlayerID const playerId) {
                assert(!is_eaten || (worm_here.has_value() && worm_here.value() == playerId));
                worm_here.emplace(playerId);
                is_eaten = true;
            }

            void remove_player(PlayerID const playerId) {
                assert(worm_here.has_value() && worm_here.value() == playerId);
                worm_here.reset();
            }
        };

        using board_t = std::vector<std::vector<Field>>;
        board_t fields;
        GameConstants const &constants;

    public:
        explicit Board(GameConstants const &constants) : constants(constants) {
            fields.resize(constants.width);
            for (auto &vec: fields) {
                vec.resize(constants.height);
            }
        }

        [[nodiscard]] bool contains(Pixel const position) const {
            return position.on_board(constants.width, constants.height);
        }

        [[nodiscard]] bool will_collide(PlayerID const playerId, Pixel const position) const {
            assert(contains(position));
            return fields[position.x][position.y].will_collide(playerId);
        }

    private:
        void put_player(PlayerID const playerId, Pixel const position) {
            assert(contains(position));
            fields[position.x][position.y].put_player(playerId);
        }

        void remove_player(PlayerID const playerId, Pixel const position) {
            assert(contains(position));
            fields[position.x][position.y].remove_player(playerId);
        }

    public:
        void init_player(PlayerID const playerId, Pixel const position) {
            put_player(playerId, position);
        }

        void move_player(PlayerID const playerId, Pixel const from, Pixel const to) {
            put_player(playerId, to);
            remove_player(playerId, from);
        }
    };
}

#endif //ROBAKI_BOARD_H
