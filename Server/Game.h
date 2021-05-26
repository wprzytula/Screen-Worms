#ifndef ROBAKI_GAME_H
#define ROBAKI_GAME_H

#include <algorithm>
#include <set>
#include <queue>

#include "Player.h"
#include "ClientData.h"
#include "../Common/Event.h"
#include "Board.h"
#include "RandomGenerator.h"

namespace Worms {
    class Game {
    private:
        GameConstants const& constants;
        Board board;
        uint32_t const game_id;
        std::vector<std::unique_ptr<Event const>> events;
        size_t next_disseminated_event_no = 0;
        std::vector<std::shared_ptr<Player>> players;
        size_t alive_players_num;
        std::vector<std::weak_ptr<Player>> observers;
        bool _finished = false;
    public:
        Game(GameConstants const& constants, RandomGenerator& rand,
             std::set<std::shared_ptr<Player>, Player::Comparator> const& ready_players,
             std::vector<std::weak_ptr<Player>> observers);

        [[nodiscard]] bool finished() const {
            return _finished;
        }

        void add_observer(std::weak_ptr<Player> const& observer) {
            observers.push_back(observer);
        }

        void play_round();
    private:
        void generate_event(uint8_t event_type, std::unique_ptr<EventDataIface> data);

        void enqueue_event_package(std::queue<UDPSendBuffer>& send_queue,
                                   size_t const next_event, UDPEndpoint receiver);

    public:
        void respond_with_events(std::queue<UDPSendBuffer>& queue, int const sock,
                                 sockaddr_in6 const& addr, uint32_t const next_event);

        void disseminate_new_events(std::queue<UDPSendBuffer>& queue, int const sock);
    };
}

#endif //ROBAKI_GAME_H
