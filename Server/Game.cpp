#include "Game.h"

namespace Worms {

    Game::Game(GameConstants const &constants, RandomGenerator &rand,
               std::set<std::shared_ptr<Player>, Player::Comparator> const &ready_players,
               std::vector<std::weak_ptr<Player>> observers)
            : constants{constants}, board{constants}, game_id{rand()},
              alive_players_num{ready_players.size()}, observers{std::move(observers)} {
        for (auto& player: ready_players) {
            player->new_game();
            players.push_back(player);
        }

        // Sort players alphabetically.
        std::sort(players.begin(), players.end(),
                  [](std::shared_ptr<Player> const& p1,
                     std::shared_ptr<Player> const& p2){
                      return p1->player_name < p2->player_name;
                  });

        {   // Generate NEW_GAME.
            std::vector<std::string> player_names;
            player_names.resize(players.size());
            for (size_t i = 0; i < players.size(); ++i) {
                player_names[i] = players[i]->player_name;
            }
            generate_event(NEW_GAME_NUM, std::make_unique<Data_NEW_GAME>(
                    constants.width, constants.height, std::move(player_names)));
        }

        // Place players at initial positions.
        for (size_t i = 0; i < players.size(); ++i) {
            auto& player = players[i];
            auto x_pos = rand() % constants.width + 0.5;
            auto y_pos = rand() % constants.height + 0.5;
            player->position.emplace(x_pos, y_pos);
            player->angle = rand() % 360;
            auto player_pixel = player->position->as_pixel();
            if (!board.contains(player_pixel) || board.is_eaten(player_pixel)) {
                generate_event(PLAYER_ELIMINATED_NUM,
                               std::make_unique<Data_PLAYER_ELIMINATED>(i));
            } else {
                board.eat(player_pixel);
                generate_event(PIXEL_NUM,std::make_unique<Data_PIXEL>(
                        i, player_pixel.x, player_pixel.y));
            }
        }
    }

    void Game::play_round() {
        for (size_t i = 0; i < players.size(); ++i) {
            auto& player = players[i];
            if (!player->is_alive())
                continue;

            if (player->turn_direction == RIGHT)
                player->angle += constants.turning_speed;
            else if (player->turn_direction == LEFT)
                player->angle -= constants.turning_speed;

            Pixel before = player->position->as_pixel();
            player->position->move_with_angle(player->angle);
            Pixel after = player->position->as_pixel();
            if (before == after) {
                continue;
            } else if (!board.contains(after) || board.is_eaten(after)) {
                player->lose();
                --alive_players_num;
                generate_event(PLAYER_ELIMINATED_NUM, std::make_unique<Data_PLAYER_ELIMINATED>(
                        Data_PLAYER_ELIMINATED{static_cast<uint8_t>(i)}));
                if (alive_players_num <= 1)
                    _finished = true;
            } else {
                board.eat(after);
                generate_event(PIXEL_NUM, std::make_unique<Data_PIXEL>(
                        Data_PIXEL{static_cast<uint8_t>(i), after.x, after.y}));
            }
        }
        if (_finished) {
            generate_event(GAME_OVER_NUM, std::make_unique<Data_GAME_OVER>());
        }
    }

    void Game::generate_event(uint8_t event_type, std::unique_ptr<EventDataIface> data) {
        uint32_t event_no = events.size();
        std::unique_ptr<Event> event;

        switch (event_type) {
            case NEW_GAME_NUM: {
                event = std::make_unique<Event_NEW_GAME>(
                        event_no, event_type, *(dynamic_cast<Data_NEW_GAME*>(data.get())));
                break;
            }
            case PIXEL_NUM: {
                event = std::make_unique<Event_PIXEL>(
                        event_no, event_type, *(dynamic_cast<Data_PIXEL*>(data.get())));
                break;
            }
            case PLAYER_ELIMINATED_NUM:
                event = std::make_unique<Event_PLAYER_ELIMINATED>(
                        event_no, event_type, *(dynamic_cast<Data_PLAYER_ELIMINATED*>(data.get())));
                break;
            case GAME_OVER_NUM:
                event = std::make_unique<Event_GAME_OVER>(
                        event_no, event_type, *(dynamic_cast<Data_GAME_OVER*>(data.get())));
                break;
            default:
                assert(false);
        }
        events.push_back(std::move(event));
    }

    void Game::enqueue_event_package(std::queue<UDPSendBuffer> &send_queue, size_t const next_event,
                                     UDPEndpoint receiver) {
        if (next_event >= events.size())
            return;

        auto* buff_ptr = &send_queue.emplace(receiver);
        buff_ptr->pack_field(game_id);

        for (auto it = events.cbegin() + static_cast<long>(next_event);
             it != events.cend(); ++it) {
            auto& event = *it;

            if (buff_ptr->remaining() < event->size()) {
                // A new buffer is needed, as the previous one is full.
                buff_ptr = &send_queue.emplace(receiver);
                buff_ptr->pack_field(game_id);
            }

            event->pack(*buff_ptr);
        }
    }

    void Game::respond_with_events(std::queue<UDPSendBuffer> &queue, int const sock,
                                   sockaddr_in6 const &addr, uint32_t const next_event) {
        enqueue_event_package(queue, next_event,UDPEndpoint{sock, addr});
    }

    void Game::disseminate_new_events(std::queue<UDPSendBuffer> &queue, int const sock) {
        for (auto& player: players) {
            if (player->is_connected()) {
                enqueue_event_package(queue, next_disseminated_event_no,
                                      UDPEndpoint{sock, player->client()->address});
            }
        }

        std::vector<decltype(observers.begin())> disconnected_observers;
        for (auto it = observers.begin(); it != observers.end(); ++it) {
            if (it->expired()) {
                disconnected_observers.push_back(it);
            } else {
                enqueue_event_package(queue, next_disseminated_event_no,
                                      UDPEndpoint{sock, it->lock()->client()->address});
            }
        }
        for (auto& disconnected: disconnected_observers) {
            observers.erase(disconnected);
        }

        next_disseminated_event_no = events.size();
    }
}

