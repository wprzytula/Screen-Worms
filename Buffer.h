#ifndef ROBAKI_BUFFER_H
#define ROBAKI_BUFFER_H

#include "defs.h"

namespace Worms {

    constexpr uint16_t const MAX_DATA_SIZE = 550;

    class UDPEndpoint {
    private:
        int const sock;
        sockaddr_in6 const address;
//        socklen_t const addr_len;
    public:
        UDPEndpoint(int const sock, sockaddr_in6 const &addr, socklen_t const addr_len)
                : sock{sock}, address{addr}/*, addr_len(addr_len)*/ {
            int err;
            verify(connect(sock, (sockaddr const *) &addr, sizeof(sockaddr_in6)), "connect");
        }

        ssize_t sendthere(void const *buff, size_t len, int flags) { // NOLINT(readability-make-member-function-const)
            return send(sock, buff, len, flags);
        }
        ssize_t recvfromthere(void *buff, int flags) { // NOLINT(readability-make-member-function-const)
            return recv(sock, buff, MAX_DATA_SIZE, flags);
        }
    };

    class UDPSendBuffer {
    private:
        char buff[MAX_DATA_SIZE] = {0};
        uint16_t _size;
        UDPEndpoint receiver;

    public:
        explicit UDPSendBuffer(UDPEndpoint receiver)
                : _size{0}, receiver{std::move(receiver)} {}

        ~UDPSendBuffer() {
            assert(_size == 0);
        }

        [[nodiscard]] uint16_t size() const {
            return _size;
        }

        [[nodiscard]] uint16_t remaining() const {
            return MAX_DATA_SIZE - _size;
        }

        void clear() {
            _size = 0;
        }

        ssize_t flush() {
            int sflags = 0;
            ssize_t res = receiver.sendthere(buff, _size, sflags);
            assert(_size == res);
            if (res != -1)
                _size = 0;
            return res;
        }

        template<typename T>
        void pack_field(T field) {
            assert(remaining() >= sizeof(T));
            switch (sizeof(T)) {
                case 8:
                    break;
                case 16:
                    field = htobe16(field);
                    break;
                case 32:
                    field = htobe32(field);
                    break;
                case 64:
                    field = htobe64(field);
                    break;
                default: // unsupported field size
                    assert(false);
            }
            *((T*)buff + _size) = field;
            _size += sizeof(T);
        }
        void pack_string(std::string const& s) {
            assert(remaining() >= s.size());
            memcpy(buff + _size, s.c_str(), s.size());
            _size += s.size();
        }
    };

    class UnendedName : public std::exception {};

    class UDPReceiveBuffer {
    private:
        static char buff[MAX_DATA_SIZE];
        uint16_t size;
        uint16_t pos;

    public:
        UDPReceiveBuffer() : size{0}, pos{0} {}

        [[nodiscard]] uint16_t remaining() const {
            return size - pos;
        }

        template<typename T>
        void unpack_field(T& field) {
            assert(remaining() >= sizeof(T));
            field = *((T*)buff + pos);
            pos += sizeof(T);
            switch (sizeof(T)) {
                case 8:
                    break;
                case 16:
                    field = be16toh(field);
                    break;
                case 32:
                    field = be32toh(field);
                    break;
                case 64:
                    field = be64toh(field);
                    break;
                default: // unsupported field size
                    assert(false);
            }
        }

        [[nodiscard]] bool exhausted() const {
            return pos == size;
        }

        bool unpack_name(std::string& s) {
            while (pos < size) {
                if (buff[pos] == ' ')
                    return true;
                else if (buff[pos] == '\0')
                    return false;
                s.push_back(buff[pos++]);
            }
            throw UnendedName{};
        }

        void unpack_remaining(std::string& s) {
            while (pos < size)
                s.push_back(buff[pos++]);
        }
    };

    class TCPSendBuffer {
    private:
        int const sock;
        size_t const initial_capacity;
        char *buff;
        size_t beg;
        size_t end;
        size_t size;
        size_t capacity;
    public:
        explicit TCPSendBuffer(int sock, size_t capacity)
            : sock{sock}, initial_capacity{capacity}, beg{0}, end{0}, size{0}, capacity{capacity} {
            buff = static_cast<char*>(malloc(capacity));
            if (buff == nullptr)
                syserr(errno, "malloc");
        }
    private:
        void grow() {
            capacity <<= 1;
            buff = static_cast<char*>(realloc(buff, capacity));
            if (buff == nullptr)
                fatal("realloc");

            memcpy(buff + capacity / 2, buff, beg);
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
            if (beg < end) {
                res = write(sock, buff + beg, size);
                if (res < 0)
                    syserr(errno, "write");
                total_written += static_cast<size_t>(res);
                if (static_cast<size_t>(res) < size) {
                    size -= total_written;
                    beg = (beg + total_written) % capacity;
                    return false;
                }
            } else {
                size_t first_portion = capacity - beg;
                res = write(sock, buff + beg, first_portion);
                if (res < 0)
                    syserr(errno, "write");
                total_written += static_cast<size_t>(res);
                if (static_cast<size_t>(res) < first_portion) {
                    size -= total_written;
                    beg = (beg + total_written) % capacity;
                    return false;
                }

                size_t second_portion = end;
                res = write(sock, buff + beg, second_portion);
                if (res < 0)
                    syserr(errno, "write");
                total_written += static_cast<size_t>(res);
                if (static_cast<size_t>(res) < second_portion) {
                    size -= total_written;
                    beg = (beg + total_written) % capacity;
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
        int const sock;
        char *buff;
        size_t beg;
        size_t end;
    };
}

#endif //ROBAKI_BUFFER_H
