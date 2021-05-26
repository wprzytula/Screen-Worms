#ifndef ROBAKI_BUFFER_H
#define ROBAKI_BUFFER_H

#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <unistd.h>

#include <optional>
#include <string>

#include "Crc32Computer.h"
#include "err.h"

namespace Worms {

    class BadData : public std::exception {};
    class Crc32Mismatch : public std::exception {};

    constexpr uint16_t const MAX_DATA_SIZE = 550;

    constexpr uint8_t const STRAIGHT = 0;
    constexpr uint8_t const RIGHT = 1;
    constexpr uint8_t const LEFT = 2;

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    static inline T htobe(T field) {
        constexpr size_t size = sizeof(field);
        if constexpr(size == 1) {
            return field;
        } else if constexpr (size == 2) {
            return htobe16(field);
        } else if constexpr (size == 4) {
            return htobe32(field);
        } else if constexpr (size == 8) {
            return htobe64(field);
        } else {
            assert(false);
        }
    }

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    static inline T betoh(T field) {
        constexpr size_t size = sizeof(field);
        if constexpr(size == 1) {
            return field;
        } else if constexpr (size == 2) {
            return be16toh(field);
        } else if constexpr (size == 4) {
            return be32toh(field);
        } else if constexpr (size == 8) {
            return be64toh(field);
        } else {
            assert(false);
        }
    }

    class UDPEndpoint {
    private:
        int const sock;
        sockaddr_in6 _address{};
        socklen_t addr_len{sizeof(sockaddr_in6)};
    public:
        UDPEndpoint(int const sock, sockaddr_in6 const &addr)
                : sock{sock}, _address{addr}, addr_len{sizeof(sockaddr_in6)} {}

        UDPEndpoint(int const sock, void *buff, size_t& size) : sock{sock} {
            ssize_t res = recvfrom(sock, buff, MAX_DATA_SIZE, 0,
                                   reinterpret_cast<sockaddr*>(&_address), &addr_len);
            verify(res, "recvfrom");
            size = res;
        }

        [[nodiscard]] sockaddr_in6 address() const {
            return _address;
        }

        ssize_t sendthere(void const *buff, size_t len) const {
            return sendto(sock, buff, len, 0, reinterpret_cast<sockaddr const *>(&_address),
                          sizeof(_address));
        }
    };

    class UDPSendBuffer {
    private:
        char buff[MAX_DATA_SIZE] = {0};
        size_t _size = 0;
        std::optional<int const> const receiver_sock;
        std::optional<UDPEndpoint> receiver;

    public:
        explicit UDPSendBuffer(int receiver_sock)
                : receiver_sock{receiver_sock} {}

        explicit UDPSendBuffer(UDPEndpoint receiver)
                : receiver{receiver} {}

        [[nodiscard]] size_t size() const {
            return _size;
        }

        [[nodiscard]] size_t remaining() const {
            return MAX_DATA_SIZE - _size;
        }

        void clear() {
            _size = 0;
        }

        bool flush();

        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
        void pack_field(T field) {
            assert(remaining() >= sizeof(T));
            field = htobe(field);
            *((T*)(buff + _size)) = field;
            _size += sizeof(T);
        }

        void pack_string(std::string const& s) {
            assert(remaining() >= s.size());
            memcpy(buff + _size, s.c_str(), s.size());
            _size += s.size();
        }

        void compute_crc(uint32_t len) {
            pack_field(Crc32Computer::compute_in_buffer(buff + _size - len, len));
        }
    };

    class UDPReceiveBuffer {
    private:
        int const sock;
        char buff[MAX_DATA_SIZE]{};
        size_t size;
        size_t pos;
        std::optional<UDPEndpoint> sender;

    public:
        explicit UDPReceiveBuffer(int const sock) : sock{sock}, size{0}, pos{0} {}

        [[nodiscard]] bool exhausted() const {
            return pos == size;
        }

        void discard() {
            size = pos = 0;
        }

        sockaddr_in6 populate() {
            assert(exhausted());
            size = pos = 0;
            sender.emplace(sock, buff, size);
            return sender.value().address();
        }

        [[nodiscard]] size_t remaining() const {
            return size - pos;
        }

        template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
        void unpack_field(T& field) {
            if (remaining() < sizeof(T))
                throw BadData{};
            field = *((T*)(buff + pos));
            pos += sizeof(T);
            field = betoh(field);
        }

        std::string unpack_name();

        void unpack_remaining(std::string& s) {
            while (pos < size)
                s.push_back(buff[pos++]);
        }

        void verify_crc32(uint32_t len_before, uint32_t len_after);
    };

    class TCPSendBuffer {
    private:
        int const sock;
        size_t const initial_capacity;
        char *buff;
        size_t beg = 0;
        size_t end = 0;
        size_t size = 0;
        size_t capacity;
    public:
        explicit TCPSendBuffer(int sock, size_t capacity)
            : sock{sock}, initial_capacity{capacity}, capacity{capacity} {
            buff = static_cast<char*>(malloc(capacity));
            if (buff == nullptr)
                syserr(errno, "malloc");
        }
        ~TCPSendBuffer() {
            free(buff);
        }
    private:
        void grow();

        void shrink();

    public:
        void pack_word(std::string const& s);

        void end_message() {
            size_t last = end == 0 ? capacity - 1 : end - 1;
            assert(buff[last] == ' ');
            buff[last] = '\n';
        }

        bool flush();
    };

    class TCPReceiveBuffer {
        static constexpr size_t const TCP_BUFF_SIZE = 256;
        static size_t const max_line_len = strlen("RIGHT_KEY_DOWN\n");
        static size_t const min_line_len = strlen("LEFT_KEY_UP\n");
        int const sock;
        char buff[TCP_BUFF_SIZE]{};
        size_t beg;
        size_t end;
        bool parsing_invalid_message = false;
    public:
        explicit TCPReceiveBuffer(int const sock) : sock{sock}, beg{0}, end{0} {}

    public:
        std::optional<std::uint8_t> fetch_direction();

        void populate();
    };
}

#endif //ROBAKI_BUFFER_H
