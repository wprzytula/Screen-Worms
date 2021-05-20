#ifndef ROBAKI_EVENT_H
#define ROBAKI_EVENT_H

#include <sys/types.h>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include "utils.h"

namespace Worms {

    /* Just to facilitate parsing incoming data */
    struct EventHeader {
        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;
    };

    constexpr size_t const event_nodata_size =
            sizeof(EventHeader::len) + sizeof(EventHeader::event_no) +
            sizeof(EventHeader::event_type) + sizeof(uint32_t);

    struct Event {
        virtual ~Event() = default;

        [[nodiscard]] virtual size_t size() const = 0;

        virtual void pack(SendBuffer& buff) = 0;

        virtual void unpack(uint8_t const** buff) = 0;
    };

    struct EventDataIface {
        virtual ~EventDataIface() = default;

        [[nodiscard]] virtual size_t size() const = 0;

        virtual void pack(SendBuffer& buff) = 0;

        virtual void unpack(uint8_t const** buff) = 0;
    };

    template<typename EventData>
    struct EventTemplate {
        static_assert(std::is_base_of_v<EventDataIface, EventData>);

        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;
        EventData event_data;
        uint32_t crc32;

        [[nodiscard]] size_t size() const {
            return event_nodata_size + event_data.size();
        }
        void pack(SendBuffer& buff) {
            pack_header(buff);
            event_data.pack(buff);
            pack_crc(buff);
        }
        void unpack(uint8_t const**const buff) {
            unpack_header(buff);
            event_data.unpack(buff);
            unpack_crc(buff);
        }
    protected:
        void pack_header(SendBuffer& buff) {
            buff.pack_field(len);
            buff.pack_field(event_no);
            buff.pack_field(event_type);
        }
        void pack_crc(SendBuffer& buff) {
            buff.pack_field(crc32);
        }
        void unpack_header(uint8_t **const buff) {
            len = *((typeof(len)*)*buff);
            *buff += sizeof(len);
            event_no = *((typeof(event_no)*)*buff);
            *buff += sizeof(event_no);
            event_type = *((typeof(event_type)*)*buff);
            *buff += sizeof(event_type);
        }
        void unpack_crc(uint8_t **const buff) {
            crc32 = *((typeof(crc32)*)*buff);
            *buff += sizeof(crc32);
        }
    };

    /* Specific event types implementation */

    /* NEW_GAME */
    constexpr uint8_t const NEW_GAME_NUM = 0;

    struct Data_NEW_GAME : public EventDataIface {
        uint32_t maxx;
        uint32_t maxy;
        std::vector<std::string> players;

        [[nodiscard]] size_t size() const override {
            return sizeof(maxx) + sizeof(maxy) + std::accumulate(
                    players.begin(), players.end(),0,
                    [](size_t sum, std::string const& s){
                        return sum += s.size();
                    });
        }
        void pack(SendBuffer& buff) override {

        }
        void unpack(uint8_t const**const buff) override {

        }
    };

    struct Event_NEW_GAME : public EventTemplate<Data_NEW_GAME> {};

    /* PIXEL */
    constexpr uint8_t const PIXEL_NUM = 1;
    struct Data_PIXEL : public EventDataIface {
        uint8_t player_number;
        uint32_t x;
        uint32_t y;

        [[nodiscard]] size_t size() const override {
            return sizeof(player_number) + sizeof(x) + sizeof(y);
        }
        void pack(SendBuffer& buff) override {
            buff.pack_field(player_number);
            buff.pack_field(x);
            buff.pack_field(y);
        }
        void unpack(uint8_t const**const buff) override {

        }
    };

    struct Event_PIXEL : public EventTemplate<Data_PIXEL> {};

    /* PLAYER_ELIMINATED */
    constexpr uint8_t const PLAYER_ELIMINATED_NUM = 2;
    struct Data_PLAYER_ELIMINATED : public EventDataIface {
        uint8_t player_number;

        [[nodiscard]] size_t size() const override {
            return sizeof(player_number);
        }
        void pack(SendBuffer& buff) override {
            buff.pack_field(player_number);
        }
        void unpack(uint8_t const**const buff) override {

        }
    };

    struct Event_PLAYER_ELIMINATED
            : public EventTemplate<Data_PLAYER_ELIMINATED> {};

    /* GAME_OVER */
    constexpr uint8_t const GAME_OVER_NUM = 3;
    struct Data_GAME_OVER : public EventDataIface {
        [[nodiscard]] size_t size() const override {
            return 0;
        }
        void pack(SendBuffer&) override {}
        void unpack(uint8_t const**const) override {}
    };

    struct Event_GAME_OVER : public EventTemplate<Data_GAME_OVER> {};
}

#endif //ROBAKI_EVENT_H
