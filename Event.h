#ifndef ROBAKI_EVENT_H
#define ROBAKI_EVENT_H

#include <numeric>

#include "defs.h"
#include "Buffer.h"
#include "Crc32Computer.h"

namespace Worms {

    class Crc32Mismatch : public std::exception {};

    struct Event {
        virtual ~Event() = default;

        [[nodiscard]] virtual size_t size() const = 0;

        virtual void pack(SendBuffer& buff) = 0;
    };

    struct EventDataIface {
        virtual ~EventDataIface() = default;

        [[nodiscard]] virtual size_t size() const = 0;

        virtual void add_to_crc32(Crc32Computer& crc32_computer) = 0;

        virtual void pack(SendBuffer& buff) = 0;
    };

    template<typename EventData>
    struct EventImpl : public Event {
        static_assert(std::is_base_of_v<EventDataIface, EventData>);
//        static_assert(std::is_convertible_v<ReceiveBuffer&, EventData>);

        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;
        EventData event_data;
        uint32_t crc32;

        // Basic construction.
        EventImpl(uint32_t len, uint32_t event_no, uint8_t event_type, EventData data)
            : len{len}, event_no{event_no}, event_type{event_type}, event_data{std::move(data)},
              crc32{compute_crc32()} {}

        // Construction from ReceiveBuffer.
        EventImpl(uint32_t len, uint32_t event_no, uint8_t event_type, ReceiveBuffer& buff)
            : len{len}, event_no{event_no}, event_type{event_type}, event_data{buff} {
                buff.unpack_field(crc32);
                if (crc32 != compute_crc32())
                    throw Crc32Mismatch{};
            }

        [[nodiscard]] size_t size() const override {
            return sizeof(len) + sizeof(event_no) + sizeof(event_type) +
                    event_data.size() + sizeof(crc32);
        }

        crc32_t compute_crc32() {
            Crc32Computer crc32_computer;
            crc32_computer.add(len);
            crc32_computer.add(event_no);
            crc32_computer.add(event_type);
            event_data.add_to_crc32(crc32_computer);
            return crc32_computer.value();
        };

        void pack(SendBuffer& buff) override {
            pack_header(buff);
            event_data.pack(buff);
            pack_crc(buff);
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
    };

    /* Specific event types implementation */

    /* NEW_GAME */
    constexpr uint8_t const NEW_GAME_NUM = 0;

    struct Data_NEW_GAME : public EventDataIface {
        uint32_t maxx{};
        uint32_t maxy{};
        std::vector<std::string> players;

        Data_NEW_GAME(uint32_t maxx, uint32_t maxy, std::vector<std::string> players)
            : maxx{maxx}, maxy{maxy}, players{std::move(players)} {}

        explicit Data_NEW_GAME(ReceiveBuffer& buff) {
            buff.unpack_field(maxx);
            buff.unpack_field(maxy);
            // TODO: players
        }

        [[nodiscard]] size_t size() const override {
            return sizeof(maxx) + sizeof(maxy) + std::accumulate(
                    players.begin(), players.end(),0,
                    [](size_t sum, std::string const& s){
                        return sum + s.size();
                    });
        }

        void add_to_crc32(Crc32Computer& crc32_computer) override {
            crc32_computer.add(maxx);
            crc32_computer.add(maxy);
            for (auto const& player: players)
                crc32_computer.add(player);
        }

        void pack(SendBuffer& buff) override {
            buff.pack_field(maxx);
            buff.pack_field(maxy);
            for (auto it = players.begin(); it != players.end(); ++it) {
                buff.pack_string(*it);
                if (std::next(it) != players.end())
                    buff.pack_field(' ');
                else
                    buff.pack_field('\0');
            }
        }
    };

    using Event_NEW_GAME = EventImpl<Data_NEW_GAME>;

    /* PIXEL */
    constexpr uint8_t const PIXEL_NUM = 1;
    struct Data_PIXEL : public EventDataIface {
        uint8_t player_number{};
        uint32_t x{};
        uint32_t y{};

        Data_PIXEL(uint8_t playerNumber, uint32_t x, uint32_t y)
            : player_number{playerNumber}, x{x}, y{y} {}

        explicit Data_PIXEL(ReceiveBuffer& receive_buffer) {
            receive_buffer.unpack_field(player_number);
            receive_buffer.unpack_field(x);
            receive_buffer.unpack_field(y);
        }

        [[nodiscard]] size_t size() const override {
            return sizeof(player_number) + sizeof(x) + sizeof(y);
        }

        void add_to_crc32(Crc32Computer& crc32_computer) override {
            crc32_computer.add(player_number);
            crc32_computer.add(x);
            crc32_computer.add(y);
        }

        void pack(SendBuffer& buff) override {
            buff.pack_field(player_number);
            buff.pack_field(x);
            buff.pack_field(y);
        }
    };

    using Event_PIXEL = EventImpl<Data_PIXEL>;

    /* PLAYER_ELIMINATED */
    constexpr uint8_t const PLAYER_ELIMINATED_NUM = 2;
    struct Data_PLAYER_ELIMINATED : public EventDataIface {
        uint8_t player_number{};

        explicit Data_PLAYER_ELIMINATED(uint8_t player_number) : player_number{player_number} {}

        explicit Data_PLAYER_ELIMINATED(ReceiveBuffer& receive_buffer) {
            receive_buffer.unpack_field(player_number);
        }

        [[nodiscard]] size_t size() const override {
            return sizeof(player_number);
        }

        void add_to_crc32(Crc32Computer& crc32_computer) override {
            crc32_computer.add(player_number);
        }

        void pack(SendBuffer& buff) override {
            buff.pack_field(player_number);
        }
    };

    using Event_PLAYER_ELIMINATED = EventImpl<Data_PLAYER_ELIMINATED>;

    /* GAME_OVER */
    constexpr uint8_t const GAME_OVER_NUM = 3;
    struct Data_GAME_OVER : public EventDataIface {
        explicit Data_GAME_OVER(ReceiveBuffer&) {}

        [[nodiscard]] size_t size() const override {
            return 0;
        }

        void add_to_crc32(Crc32Computer&) override {}

        void pack(SendBuffer&) override {}
    };

    using Event_GAME_OVER = EventImpl<Data_GAME_OVER>;


    /* Facilitates parsing incoming data. */
    class EventParser {
        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;

        std::unique_ptr<Event> unpack(ReceiveBuffer& buff) {
            buff.unpack_field(len);
            buff.unpack_field(event_no);
            buff.unpack_field(event_type);

            std::unique_ptr<Event> res;

            switch (event_type) {
                case NEW_GAME_NUM:
                    res = std::make_unique<Event_NEW_GAME>(len, event_no, event_type, buff);
                    break;
                case PIXEL_NUM:
                    res = std::make_unique<Event_PIXEL>(len, event_no, event_type, buff);
                    break;
                case PLAYER_ELIMINATED_NUM:
                    res = std::make_unique<Event_PLAYER_ELIMINATED>(len, event_no, event_type, buff);
                    break;
                case GAME_OVER_NUM:
                    res = std::make_unique<Event_GAME_OVER>(len, event_no, event_type, buff);
                    break;
            }
            return res;
        }
    };
}

#endif //ROBAKI_EVENT_H
