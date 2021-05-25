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
                return ClientData::Comparator()(*p1._client, *p2._client);
            }
        };
        struct PtrComparator {
            using is_transparent = void;

            bool operator()(std::shared_ptr<Player> const &p1, std::shared_ptr<Player> const &p2) const {
                return ClientData::Comparator()(*p1->_client, *p2->_client);
            }
        };
    private:
        std::shared_ptr<ClientData> _client;
    public:
        std::string const& player_name;
        bool mutable ready = false;
    private:
        bool connected = true;
    public:
        uint8_t mutable turn_direction;
        std::optional<Position> position;
        angle_t angle = 0;

        Player(std::shared_ptr<ClientData> client, std::string const& player_name,
               uint8_t turn_direction)
                : _client{std::move(client)}, player_name{player_name}, turn_direction{turn_direction} {}

        bool is_connected() const {
            return connected;
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
