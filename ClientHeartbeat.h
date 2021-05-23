#ifndef ROBAKI_CLIENTHEARTBEAT_H
#define ROBAKI_CLIENTHEARTBEAT_H

#include <utility>

#include "defs.h"
#include "Buffer.h"

namespace Worms {
    class ClientHeartbeat {
        uint64_t session_id{};
        uint8_t turn_direction{};
        uint32_t next_expected_event_no{};
        std::string player_name;

    public:
        ClientHeartbeat(uint64_t session_id, uint8_t turn_direction,
                        uint32_t next_expected_event_no, std::string player_name)
                : session_id{session_id}, turn_direction{turn_direction},
                  next_expected_event_no{next_expected_event_no},
                  player_name{std::move(player_name)} {}

        explicit ClientHeartbeat(UDPReceiveBuffer &buff) {
            buff.unpack_field(session_id);
            buff.unpack_field(turn_direction);
            buff.unpack_field(next_expected_event_no);
            buff.unpack_remaining(player_name);
        }

        void pack(UDPSendBuffer &buff) {
            buff.pack_field(session_id);
            buff.pack_field(turn_direction);
            buff.pack_field(next_expected_event_no);
            buff.pack_string(player_name);
        }
    };
}

#endif //ROBAKI_CLIENTHEARTBEAT_H
