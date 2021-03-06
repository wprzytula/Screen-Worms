#ifndef ROBAKI_EVENT_H
#define ROBAKI_EVENT_H

#include <numeric>
#include <memory>

#include "Buffer.h"
#include "Crc32Computer.h"

namespace Worms {

    class UnknownEventType : public std::exception {};

    struct Event {
        struct Comparator {
            using is_transparent = void;
            bool operator()(std::unique_ptr<Event> const& ptr1,
                    std::unique_ptr<Event> const& ptr2) const {
                return ptr1->event_no < ptr2->event_no;
            }
        };

        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;

        Event(uint32_t len, uint32_t event_no, uint8_t event_type)
            : len{len}, event_no{event_no}, event_type{event_type} {}

        virtual ~Event() = default;

        [[nodiscard]] virtual size_t size() const = 0;

        virtual void pack(UDPSendBuffer& buff) const = 0;

        virtual void check_validity(std::vector<std::string> const& players,
                               uint32_t board_width, uint32_t board_height) = 0;

        virtual void stringify(TCPSendBuffer& buff, std::vector<std::string> const& players) const = 0;
    };

    struct EventDataIface {
        virtual ~EventDataIface() = default;

        [[nodiscard]] virtual size_t size() const = 0;

        virtual void pack(UDPSendBuffer& buff) const = 0;

        virtual void check_validity(std::vector<std::string> const& players, uint32_t board_width,
                            uint32_t board_height) = 0;

        virtual void stringify(TCPSendBuffer& buff,
                               std::vector<std::string> const& players) const = 0;

        virtual void pack_name(TCPSendBuffer& buff) const = 0;
    };

    template<typename EventData,
            typename = std::enable_if_t<std::is_base_of_v<EventDataIface, EventData>>>
    struct EventImpl : public Event {

        EventData event_data;
        uint32_t crc32{};

        // Basic construction.
        EventImpl(uint32_t event_no, uint8_t event_type, EventData data)
            : Event{static_cast<uint32_t>(sizeof(event_no) + sizeof(event_type) + data.size()),
                    event_no, event_type},
              event_data{std::move(data)} {}

        // Construction from UDPReceiveBuffer.
        EventImpl(uint32_t len, uint32_t event_no, uint8_t event_type, UDPReceiveBuffer& buff)
            : Event{len, event_no, event_type},
              event_data{buff, len - static_cast<uint32_t>(sizeof(event_no) + sizeof(event_type))} {
                buff.unpack_field(crc32);
            }

        [[nodiscard]] size_t size() const override {
            return sizeof(len) + sizeof(event_no) + sizeof(event_type) +
                    event_data.size() + sizeof(crc32);
        }

        void pack(UDPSendBuffer& buff) const override {
            buff.pack_field(len);
            buff.pack_field(event_no);
            buff.pack_field(event_type);
            event_data.pack(buff);
            buff.compute_crc(len + sizeof(len));
        }

        void check_validity(std::vector<std::string> const& players, uint32_t board_width,
                            uint32_t board_height) override {
            event_data.check_validity(players, board_width, board_height);
        }

        void stringify(TCPSendBuffer& buff,
                       std::vector<std::string> const& players) const override {
            event_data.pack_name(buff);
            event_data.stringify(buff, players);
            buff.end_message();
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

        Data_NEW_GAME(UDPReceiveBuffer& buff, uint32_t len) {
            buff.unpack_field(maxx);
            buff.unpack_field(maxy);

            len -= sizeof(maxx) + sizeof(maxy);
            while (len > 0 && len < MAX_DATA_SIZE) {
                players.push_back(buff.unpack_name());
                len -= players[players.size() - 1].size() + 1;
            }
        }

        [[nodiscard]] size_t size() const override {
            return sizeof(maxx) + sizeof(maxy) + std::accumulate(
                    players.begin(), players.end(),0,
                    [](size_t sum, std::string const& s){
                        return sum + s.size() + 1;
                    });
        }

        void pack(UDPSendBuffer& buff) const override {
            buff.pack_field(maxx);
            buff.pack_field(maxy);
            for (auto const& player : players) {
                buff.pack_string(player);
                buff.pack_field('\0');
            }
        }

        void pack_name(TCPSendBuffer &buff) const override {
            buff.pack_word("NEW_GAME");
        }

        void check_validity(std::vector<std::string> const &, uint32_t,
                            uint32_t) override {}

        void stringify(TCPSendBuffer &buff, std::vector<std::string> const&) const override {
            buff.pack_word(std::to_string(maxx));
            buff.pack_word(std::to_string(maxy));
            for (auto const& player : players) {
                buff.pack_word(player);
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

        Data_PIXEL(UDPReceiveBuffer& buff, uint32_t) {
            buff.unpack_field(player_number);
            buff.unpack_field(x);
            buff.unpack_field(y);
        }

        [[nodiscard]] size_t size() const override {
            return sizeof(player_number) + sizeof(x) + sizeof(y);
        }

        void pack(UDPSendBuffer& buff) const override {
            buff.pack_field(player_number);
            buff.pack_field(x);
            buff.pack_field(y);
        }

        void pack_name(TCPSendBuffer &buff) const override {
            buff.pack_word("PIXEL");
        }

        void check_validity(std::vector<std::string> const &players, uint32_t board_width,
                            uint32_t board_height) override {
            if (player_number >= players.size())
                fatal("Invalid player number received from server!");
            if (x >= board_width || y >= board_height)
                fatal("Field out of bounds given by server!");
        }

        void stringify(TCPSendBuffer &buff,
                       std::vector<std::string> const& players) const override {
            buff.pack_word(std::to_string(x));
            buff.pack_word(std::to_string(y));
            buff.pack_word(players[player_number]);
        }
    };

    using Event_PIXEL = EventImpl<Data_PIXEL>;

    /* PLAYER_ELIMINATED */
    constexpr uint8_t const PLAYER_ELIMINATED_NUM = 2;
    struct Data_PLAYER_ELIMINATED : public EventDataIface {
        uint8_t player_number{};

        explicit Data_PLAYER_ELIMINATED(uint8_t player_number) : player_number{player_number} {}

        Data_PLAYER_ELIMINATED(UDPReceiveBuffer& buff, uint32_t) {
            buff.unpack_field(player_number);
        }

        [[nodiscard]] size_t size() const override {
            return sizeof(player_number);
        }

        void pack(UDPSendBuffer& buff) const override {
            buff.pack_field(player_number);
        }

        void pack_name(TCPSendBuffer &buff) const override {
            buff.pack_word("PLAYER_ELIMINATED");
        }

        void check_validity(std::vector<std::string> const &players, uint32_t, uint32_t) override {
            if (player_number >= players.size())
                fatal("Invalid player number received from server!");
        }

        void stringify(TCPSendBuffer &buff,
                       std::vector<std::string> const& players) const override {
            buff.pack_word(players[player_number]);
        }
    };

    using Event_PLAYER_ELIMINATED = EventImpl<Data_PLAYER_ELIMINATED>;

    /* GAME_OVER */
    constexpr uint8_t const GAME_OVER_NUM = 3;
    struct Data_GAME_OVER : public EventDataIface {
        Data_GAME_OVER() = default;

        Data_GAME_OVER(UDPReceiveBuffer&, uint32_t) {}

        [[nodiscard]] size_t size() const override {
            return 0;
        }

        void pack(UDPSendBuffer&) const override {}

        void pack_name(TCPSendBuffer &) const override {}

        void check_validity(std::vector<std::string> const &, uint32_t,
                            uint32_t) override {}

        void stringify(TCPSendBuffer &, std::vector<std::string> const&) const override {}
    };

    using Event_GAME_OVER = EventImpl<Data_GAME_OVER>;

    /* Facilitates parsing incoming data. */
    inline std::unique_ptr<Event> unpack_event(UDPReceiveBuffer& buff) {
        uint32_t len;
        uint32_t event_no;
        uint8_t event_type;

        buff.unpack_field(len);

        // Here Crc32Mismatch exception is thrown in case of bad crc.
        buff.verify_crc32(sizeof(len), len);

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
            default:
                throw UnknownEventType{};
        }
        return res;
    }
}

#endif //ROBAKI_EVENT_H
