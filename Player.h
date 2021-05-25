#ifndef ROBAKI_PLAYER_H
#define ROBAKI_PLAYER_H

#include <utility>

#include "defs.h"
#include "ClientData.h"
#include "Board.h"

namespace Worms {
    class Player {
    public:
        struct Comparator {
            using is_transparent = void;

            bool operator()(Player const &p1, Player const &p2) const {
                return ClientData::Comparator()(*p1._client, *p2._client);
            }

            bool operator()(std::shared_ptr<Player> const &p1, std::shared_ptr<Player> const &p2) const {
                return operator()(*p1, *p2);
            }
            bool operator()(Player const &p1, std::shared_ptr<Player> const &p2) const {
                return operator()(p1, *p2);
            }
            bool operator()(std::shared_ptr<Player> const &p1, Player const &p2) const {
                return operator()(*p1, p2);
            }
        };
    private:
        std::shared_ptr<ClientData> _client;
    public:
        std::string const player_name;
    private:
        bool mutable ready = false;
        bool connected = false;
        bool alive = true;
    public:
        uint8_t mutable turn_direction;
        std::optional<Position> position;
        angle_t angle = 0;

        Player(std::string player_name, uint8_t turn_direction)
                : player_name{std::move(player_name)}, turn_direction{turn_direction} {}

        void attach_to_client(std::shared_ptr<ClientData> client) {
            connected = true;
            _client = std::move(client);
        }

        bool is_observer() const {
            return player_name.empty();
        }

        bool is_connected() const {
            return connected;
        }

        bool is_ready() const {
            return connected;
        }

        bool is_alive() const {
            return alive;
        }

        void got_ready() {
            ready = true;
        }

        void new_game() {
            ready = false;
            alive = true;
        }

        void lose() {
            alive = false;
        }

        void disconnect() {
            assert(connected);
            connected = false;
            _client.reset();
        }

        std::shared_ptr<ClientData>& client() {
            assert(connected);
            return _client;
        }
    };
}

#endif //ROBAKI_PLAYER_H
