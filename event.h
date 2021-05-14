#ifndef ROBAKI_EVENT_H
#define ROBAKI_EVENT_H

#include <sys/types.h>
//#include <unistd.h>
//#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

namespace Worms {

    /* Just to facilitate parsing incoming data */
    struct __attribute__((packed)) EventHeader {
        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;
    };

    struct __attribute__((packed)) Event {
        virtual ~Event() = default;
        [[nodiscard]] virtual size_t size() const = 0;
        virtual void pack(void *buff) const = 0;
        virtual void unpack(void const *buff) = 0;
    };

    template<typename EventData>
    struct __attribute__((packed)) EventTemplate {
        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;
        EventData event_data;
        uint32_t crc32;
    };

    /* Specific event types implementation */

    /* NEW_GAME */
    constexpr uint8_t const NEW_GAME_NUM = 0;

    struct __attribute__((packed)) Data_NEW_GAME {
        uint32_t maxx;
        uint32_t maxy;
        std::vector<std::string> players;
    };

    using Event_NEW_GAME = EventTemplate<Data_NEW_GAME>;

    template<>
    struct EventTemplate<Data_NEW_GAME> {
        void pack(void *buff) {

        }
    };

    /* PIXEL */
    constexpr uint8_t const PIXEL_NUM = 1;
    struct __attribute__((packed)) Data_PIXEL {
        uint8_t player_number;
        uint32_t x;
        uint32_t y;
    };

    using Event_PIXEL = EventTemplate<Data_PIXEL>;

    template<>
    struct EventTemplate<Data_PIXEL> {
        void pack(void *buff) {

        }
    };

    /* PLAYER_ELIMINATED */
    constexpr uint8_t const PLAYER_ELIMINATED_NUM = 2;
    struct __attribute__((packed)) Data_PLAYER_ELIMINATED {
        uint8_t player_number;
    };

    using Event_PLAYER_ELIMINATED = EventTemplate<Data_PLAYER_ELIMINATED>;

    template<>
    struct EventTemplate<Data_PLAYER_ELIMINATED> {
        void pack(void *buff) {

        }
    };

    /* GAME_OVER */
    constexpr uint8_t const GAME_OVER_NUM = 3;
    struct __attribute__((packed)) Data_GAME_OVER {
        uint8_t player_number;
    };

    using Event_GAME_OVER = EventTemplate<Data_GAME_OVER>;

    template<>
    struct EventTemplate<Data_GAME_OVER> {
        void pack(void *buff) {

        }
    };
}

#endif //ROBAKI_EVENT_H
