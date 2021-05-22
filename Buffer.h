#ifndef ROBAKI_BUFFER_H
#define ROBAKI_BUFFER_H

#include "defs.h"

namespace Worms {

    constexpr uint16_t const MAX_DATA_SIZE = 550;

    class Receiver {
    private:
        int const sock;
        sockaddr_in6 const address;
        socklen_t const addr_len;
    public:
        Receiver(int const sock, sockaddr_in6 const &addr, socklen_t const addr_len)
                : sock{sock}, address{addr}, addr_len(addr_len) {
            int err;
            verify(connect(sock, (sockaddr const *) &addr, addr_len), "connect");
        }

        ssize_t sendthere(void const *buff, size_t len, int flags) { // NOLINT(readability-make-member-function-const)
            return send(sock, buff, len, flags);
        }
    };

    class SendBuffer {
    private:
        char buff[MAX_DATA_SIZE] = {0};
        uint16_t _size;
        Receiver receiver;

    public:
        SendBuffer(uint16_t const size, Receiver receiver)
                : _size{0}, receiver{std::move(receiver)} {}

        ~SendBuffer() {
            assert(_size == 0);
        }

        [[nodiscard]] uint16_t size() const {
            return _size;
        }

        [[nodiscard]] uint16_t remaining() const {
            return MAX_DATA_SIZE - _size;
        }

        ssize_t flush() {
            int sflags = 0;
            ssize_t res = receiver.sendthere(buff, _size, sflags);
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

    class ReceiveBuffer {
    private:
        static char buff[MAX_DATA_SIZE];
        uint16_t size;
        uint16_t pos;

    public:
        ReceiveBuffer() : size{0}, pos{0} {}

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
}

#endif //ROBAKI_BUFFER_H
