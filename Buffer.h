#ifndef ROBAKI_BUFFER_H
#define ROBAKI_BUFFER_H

#include <arpa/inet.h>
#include "defs.h"
#include "Crc32Computer.h"
#include <iostream>

namespace Worms {

    class BadData : public std::exception {};
    class Crc32Mismatch : public std::exception {};

    constexpr uint16_t const MAX_DATA_SIZE = 550;

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

        bool flush() {
            ssize_t res;
            if (receiver.has_value())
                res = receiver->sendthere(buff, _size);
            else
                res = send(*receiver_sock, buff, _size, 0);
//            fprintf(stderr, "Sent %ld bytes\n", res);
            if (res == -1) {
                if (!(errno == EAGAIN || errno == EWOULDBLOCK))
                    syserr(errno, "cannot send to remote host (UDP)");
                return false;
            } else {
                _size = 0;
                return true;
            }
        }

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

        std::string unpack_name() {
            std::string s;
            while (pos < size) {
                if (buff[pos] == '\0') {
                    ++pos;
                    return s;
                }
                s.push_back(buff[pos++]);
            }
            throw BadData{};
        }

        void unpack_remaining(std::string& s) {
            while (pos < size)
                s.push_back(buff[pos++]);
        }

        void verify_crc32(uint32_t len_before, uint32_t len_after) {
            if (pos + len_after + sizeof(crc32_t) > size)
                throw BadData{};
            crc32_t computed = Crc32Computer::compute_in_buffer(buff + pos - len_before,
                                                            len_before + len_after);
            crc32_t received = betoh(*reinterpret_cast<crc32_t*>(buff + pos + len_after));
            if (computed != received)
                throw Crc32Mismatch{};
        }
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
        void grow() {
            capacity <<= 1;
            buff = static_cast<char*>(realloc(buff, capacity));
            if (buff == nullptr)
                fatal("realloc");

            memcpy(buff + (capacity >> 2), buff, beg);
            end = (beg + size) % capacity;
        }

        void shrink() {
            assert(capacity > initial_capacity);
            capacity = initial_capacity;
            buff = static_cast<char*>(realloc(buff, initial_capacity));
            if (buff == nullptr)
                fatal("realloc failed");
        }

    public:
        void pack_word(std::string const& s) {
            if (capacity - size < s.size() + 1)
                grow();
            char const *ptr = s.c_str();
            if (capacity - end < s.size()) {
                size_t first_portion = capacity - end;
                memcpy(buff + end, ptr, first_portion);
                memcpy(buff, ptr + first_portion, s.size() - first_portion);
            } else {
                memcpy(buff + end, ptr, s.size());
            }
            end = (end + s.size()) % capacity;
            buff[end] = ' ';
            end = (end + 1) % capacity;
            size += s.size() + 1;
        }
        void end_message() {
            size_t last = end == 0 ? capacity - 1 : end - 1;
            assert(buff[last] == ' ');
            buff[last] = '\n';
        }

        bool flush() {
            // Up to 2 subsequent writes: beg->capacity and end->beg
            if (size == 0)
                return true;
            ssize_t res;
            size_t total_written = 0;
//            printf("Iface send buffer contains:\n");
            if (beg < end) {
//                for (size_t i = beg; i < end; ++i) {
//                    putchar_unlocked(buff[i]);
//                }
                res = write(sock, buff + beg, size);
                if (res < 0)
                    syserr(errno, "write to iface");
                total_written += static_cast<size_t>(res);
                if (static_cast<size_t>(res) < size) {
                    size -= total_written;
                    beg = (beg + total_written) % capacity;
//                    putchar_unlocked('\n');
                    return false;
                }
            } else {
//                for (size_t i = beg; i < capacity; ++i) {
//                    putchar_unlocked(buff[i]);
//                }
                size_t first_portion = capacity - beg;
                res = write(sock, buff + beg, first_portion);
                if (res < 0)
                    syserr(errno, "write to iface");
                total_written += static_cast<size_t>(res);
                if (static_cast<size_t>(res) < first_portion) {
                    size -= total_written;
                    beg = (beg + total_written) % capacity;
//                    putchar_unlocked('\n');
                    return false;
                }

//                for (size_t i = 0; i < end; ++i) {
//                    putchar_unlocked(buff[i]);
//                }
                size_t second_portion = end;
                res = write(sock, buff + beg, second_portion);
                if (res < 0)
                    syserr(errno, "write to iface");
                total_written += static_cast<size_t>(res);
                if (static_cast<size_t>(res) < second_portion) {
                    size -= total_written;
                    beg = (beg + total_written) % capacity;
//                    putchar_unlocked('\n');
                    return false;
                }
            }
            assert(total_written == size);
            // If we are here, the buffer has been emptied, so we can reset it.
            beg = end = 0;
            size = 0;
            if (capacity > initial_capacity)
                shrink();
            return true;
        }
    };

    class TCPReceiveBuffer {
        static constexpr size_t const TCP_BUFF_SIZE = 256;
        static size_t const max_line_len = strlen("RIGHT_KEY_DOWN\n");
        static size_t const min_line_len = strlen("LEFT_KEY_UP\n");
        int const sock;
        char buff[TCP_BUFF_SIZE]{};
        size_t beg;
        size_t end;
    public:
        explicit TCPReceiveBuffer(int const sock) : sock{sock}, beg{0}, end{0} {}

        [[nodiscard]] bool has_data() const {
            return end - beg >= max_line_len ||
                (end - beg >= min_line_len && (
                    buff[beg + min_line_len - 1] == '\n' ||
                    buff[beg + min_line_len] == '\n' ||
                    buff[beg + min_line_len + 1] == '\n')
                );
        }

        uint8_t fetch_next() {
            static char const* messages[]{"RIGHT_KEY_DOWN\n", "RIGHT_KEY_UP\n",
                                           "LEFT_KEY_DOWN\n", "LEFT_KEY_UP\n"};
            static size_t const messages_len[]{
                strlen("RIGHT_KEY_DOWN\n"), strlen("RIGHT_KEY_UP\n"),
                    strlen("LEFT_KEY_DOWN\n"), strlen("LEFT_KEY_UP\n")};
            static uint8_t const turn_direction[]{RIGHT, STRAIGHT, LEFT, STRAIGHT};

            for (size_t i = 0; i < sizeof(messages) / sizeof(char const *); ++i) {
                if (strncmp(buff + beg, messages[i], std::min(messages_len[i], end - beg)) == 0) {
                    beg += messages_len[i];
                    return turn_direction[i];
                }
            }
            fatal("Invalid message received from interface!");
            return 3;
        }

        void populate() {
            if (beg != end) {
                char tmp[max_line_len];
                memcpy(tmp, buff + beg, end - beg);
                memcpy(buff, tmp, end - beg);
                end = end - beg;
                beg = 0;
            } else {
                beg = end = 0;
            }
            ssize_t res = read(sock, buff + end, TCP_BUFF_SIZE - end);
            verify(res, "read");
            end += res;
        }

//        void print() {
//            printf("Buffer receive iface contains:\n");
//            for (size_t i = beg; i < end; ++i) {
//                putchar_unlocked(buff[i]);
//            }
//        }
    };
}

#endif //ROBAKI_BUFFER_H
