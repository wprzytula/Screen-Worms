#ifndef ROBAKI_GAME_H
#define ROBAKI_GAME_H

#include "defs.h"
#include "Player.h"
#include "ClientData.h"
#include "Event.h"
#include "GameConstants.h"
#include "Board.h"
#include "RandomGenerator.h"

namespace Worms {
    class Game {
    private:
        GameConstants const& constants;
        Board board;
        RandomGenerator& rand;
        uint32_t const game_id;
        std::vector<std::unique_ptr<Event const>> events;
        size_t next_disseminated_event_no = 0;
        std::vector<std::shared_ptr<Player>> players;
        size_t alive_players_num;
        std::set<std::weak_ptr<Player>> observers;
        bool _finished = false;
    public:
        Game(GameConstants const& constants, RandomGenerator& rand,
             std::set<std::shared_ptr<Player>> const& ready_players,
             std::set<std::weak_ptr<Player>> observers)
                : constants{constants}, board{constants}, rand{rand}, game_id{rand()},
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
                player->position.emplace(rand() % constants.width + 0.5,
                                         rand() % constants.height + 0.5);
                player->angle = rand() % 360;
                auto player_pixel = player->position->as_pixel();
                if (board.is_eaten(player_pixel)) {
                    generate_event(PLAYER_ELIMINATED_NUM,
                                   std::make_unique<Data_PLAYER_ELIMINATED>(i));
                } else {
                    board.eat(player_pixel);
                    generate_event(PIXEL_NUM,std::make_unique<Data_PIXEL>(
                            i, player_pixel.x, player_pixel.y));
                }
            }
        }

        [[nodiscard]] bool finished() const {
            return _finished;
        }

        void add_observer(std::weak_ptr<Player> const& observer) {
            observers.insert(observer);
        }

        void play_round() {
            for (size_t i = 0; i < players.size(); ++i) {
                auto& player = players[i];
                if (!player->is_alive())
                    continue;

                if (player->turn_direction == 1)
                    player->angle += constants.turning_speed;
                else if (player->turn_direction == 2)
                    player->angle -= constants.turning_speed;

                Pixel before = player->position->as_pixel();
                player->position->move_with_angle(player->angle);
                Pixel after = player->position->as_pixel();
                if (before == after) {
                    continue;
                } else if (board.is_eaten(after)) {
                    player->lose();
                    --alive_players_num;
                    generate_event(PLAYER_ELIMINATED_NUM, std::make_unique<Data_PLAYER_ELIMINATED>(
                            Data_PLAYER_ELIMINATED{static_cast<uint8_t>(i)}));
                } else {
                    board.eat(after);
                    generate_event(PIXEL_NUM, std::make_unique<Data_PIXEL>(
                            Data_PIXEL{static_cast<uint8_t>(i), after.x, after.y}));
                }

                if (alive_players_num == 0) {
                    generate_event(GAME_OVER_NUM, std::make_unique<Data_GAME_OVER>());
                    _finished = true;
                }
            }
        }
    private:
        void generate_event(uint8_t event_type, std::unique_ptr<EventDataIface> data) {
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

        void enqueue_event_package(std::queue<UDPSendBuffer>& send_queue,
                                   size_t const next_event, UDPEndpoint receiver) {
            if (next_event >= events.size())
                return;

            send_queue.emplace(receiver);
            for (auto it = events.begin() + static_cast<long>(next_event);
                 it != events.end(); ++it) {
                auto& event = *it;
                if (send_queue.back().remaining() < event->size()) {
                    send_queue.emplace(receiver); // A new buffer is needed, as the previous one is full.
                }
                event->pack(send_queue.back());
            }
        }

    public:
        void disseminate_new_events(std::queue<UDPSendBuffer>& queue, int const sock) {
            for (auto& player: players) {
                if (player->is_connected()) {
                    enqueue_event_package(queue, next_disseminated_event_no,
                                          UDPEndpoint{sock, player->client()->address});
                }
            }
            std::vector<typeof(observers.begin())> disconnected_observers;
            for (auto it = observers.begin(); it != observers.end(); ++it) {
                if (it->expired()) {
                    disconnected_observers.push_back(it);
                } else {
                    enqueue_event_package(queue, next_disseminated_event_no,
                                          UDPEndpoint{sock, it->lock()->client()->address});
                }
            }
            for (auto it: disconnected_observers) {
                observers.erase(it);
            }
        }
    };
}

#endif //ROBAKI_GAME_H
