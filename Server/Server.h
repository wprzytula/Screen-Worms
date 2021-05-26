#ifndef ROBAKI_SERVER_H
#define ROBAKI_SERVER_H

#include <fcntl.h>
#include <sys/timerfd.h>

#include <queue>
#include <set>

#include "../Common/Buffer.h"
#include "RandomGenerator.h"
#include "../Common/Epoll.h"
#include "../Common/ClientHeartbeat.h"
#include "Player.h"
#include "Game.h"

namespace Worms {
    class Server {
    private:
        static constexpr uint64_t const NS_IN_SEC = 1'000'000'000;
        static constexpr uint64_t const DISCONNECT_THRESHOLD = 2 * NS_IN_SEC;

        int const sock;
        int const round_timer;
        Epoll epoll;
        uint64_t round_no = 0;
        RandomGenerator rand;
        GameConstants const constants;
        uint64_t const round_duration_ns;
        std::optional<Game> current_game;
        std::optional<Game> previous_game;
        std::queue<UDPSendBuffer> send_queue;
        UDPReceiveBuffer receive_buff;

        std::set<std::shared_ptr<ClientData>, ClientData::Comparator> connected_clients;
        std::set<std::shared_ptr<Player>, Player::Comparator> connected_players;
        std::set<std::shared_ptr<Player>, Player::Comparator> connected_unnames;
        std::set<std::string> player_names;

    public:
        explicit Server(uint16_t const port, uint32_t const seed, GameConstants constants);

        ~Server() {
            close(sock);
            close(round_timer);
        }
    private:
        void disconnect_idles();

        void round_routine();

        void try_start_game();

        bool drain_queue() {
            while (!send_queue.empty()) {
                if (send_queue.front().flush())
                    send_queue.pop();
                else
                    return false;
            }
            return true;
        }

        void handle_heartbeat();

        void connect_client(sockaddr_in6 const& addr, ClientHeartbeat heartbeat);

        void disconnect_client(decltype(connected_clients.begin()) client_ptr);

    public:
        [[noreturn]] void mainloop();
    };
}

#endif //ROBAKI_SERVER_H
