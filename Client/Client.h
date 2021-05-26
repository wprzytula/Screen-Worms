#ifndef ROBAKI_CLIENT_H
#define ROBAKI_CLIENT_H

#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <sys/timerfd.h>

#include <set>
#include <vector>

#include "../Common/Epoll.h"
#include "../Common/Event.h"

namespace Worms {

    int gai_sock_factory(int sock_type, char const *name, uint16_t port);

    class Client {
    private:
        static constexpr long const COMMUNICATION_INTERVAL = 30'000'000;
        static constexpr long const INITIAL_IFACE_BUFF_CAP = 256;

        uint64_t const session_id;
        std::string const player_name;
        int const server_sock;
        int const iface_sock;
        int const heartbeat_timer;

        Epoll epoll;
        UDPSendBuffer server_send_buff;
        UDPReceiveBuffer server_receive_buff;
        TCPSendBuffer iface_send_buff;
        TCPReceiveBuffer iface_receive_buff;
        uint8_t turn_direction = STRAIGHT;
        uint32_t next_expected_event_no = 0;
        std::set<std::unique_ptr<Event>, Event::Comparator> future_events;
        std::vector<std::string> players;
        uint32_t board_width{}, board_height{};
        uint32_t current_game_id{};
        std::set<uint32_t> previous_game_ids;


    public:
        Client(std::string player_name, char const *game_server, uint16_t server_port,
               char const *game_iface, uint16_t iface_port);

        ~Client() {
            close(server_sock);
            close(iface_sock);
            close(heartbeat_timer);
        }

    private:
        /* Reacts accordingly to message from GUI interface. */
        void handle_iface_msg();

        /* Sends periodical signal to server filled with status data. */
        void send_heartbeat();

        /* Should it happened that server socket clogged up,
         * here we later send the enqueued heartbeat after it becomes usable again. */
        void drain_server_queue() {
            epoll.stop_watching_fd_for_output(server_sock);
            server_send_buff.flush();
        }

        /* Receives and parses new events, then resends them to GUI. */
        void handle_events();

    public:
        /* Main client routine. Endless loop of sending heartbeat
         * and responding to messages from server and iface. */
        [[noreturn]] void play();
    };
}

#endif //ROBAKI_CLIENT_H
