#include "Buffer.h"

namespace Worms {
    void TCPSendBuffer::pack_word(std::string const &s) {
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

    bool TCPSendBuffer::flush() {
        // Up to 2 subsequent writes: beg->capacity and end->beg
        if (size == 0)
            return true;
        ssize_t res;
        size_t total_written = 0;
        if (beg < end) {
            res = write(sock, buff + beg, size);
            if (res < 0)
                syserr(errno, "write to iface");
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
                syserr(errno, "write to iface");
            total_written += static_cast<size_t>(res);
            if (static_cast<size_t>(res) < first_portion) {
                size -= total_written;
                beg = (beg + total_written) % capacity;
                return false;
            }

            size_t second_portion = end;
            res = write(sock, buff + beg, second_portion);
            if (res < 0)
                syserr(errno, "write to iface");
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

    void TCPSendBuffer::grow() {
        capacity <<= 1;
        buff = static_cast<char*>(realloc(buff, capacity));
        if (buff == nullptr)
            fatal("realloc");

        memcpy(buff + (capacity >> 2), buff, beg);
        end = (beg + size) % capacity;
    }

    void TCPSendBuffer::shrink() {
        assert(capacity > initial_capacity);
        capacity = initial_capacity;
        buff = static_cast<char*>(realloc(buff, initial_capacity));
        if (buff == nullptr)
            fatal("realloc failed");
    }

    std::optional<std::uint8_t> TCPReceiveBuffer::fetch_direction() {
        while (end - beg >= min_line_len) {
            if (parsing_invalid_message) { // skip invalid message
                while (beg < end) {
                    if (buff[beg++] == '\n') {
                        parsing_invalid_message = false;
                        break;
                    }
                }
            } else {
                std::optional<size_t> newline;
                for (size_t i = 0; i < std::min(end - beg, max_line_len); ++i) {
                    if (buff[beg + i] == '\n') {
                        newline.emplace(i);
                        break;
                    }
                }
                if (newline.has_value()) {
                    switch (*newline) {
                        case 14: // strlen("RIGHT_KEY_DOWN\n")
                            if (strncmp(buff + beg, "RIGHT_KEY_DOWN\n", 15) == 0) {
                                beg += *newline + 1;
                                return std::make_optional(RIGHT);
                            }
                            break;
                        case 12: // strlen("RIGHT_KEY_UP\n")
                            if (strncmp(buff + beg, "RIGHT_KEY_UP\n", 13) == 0) {
                                beg += *newline + 1;
                                return std::make_optional(STRAIGHT);
                            }
                            break;
                        case 13: // strlen("LEFT_KEY_DOWN\n")
                            if (strncmp(buff + beg, "LEFT_KEY_DOWN\n", 14) == 0) {
                                beg += *newline + 1;
                                return std::make_optional(LEFT);
                            }
                            break;
                        case 11: // strlen("LEFT_KEY_UP\n")
                            if (strncmp(buff + beg, "LEFT_KEY_UP\n", 12) == 0) {
                                beg += *newline + 1;
                                return std::make_optional(STRAIGHT);
                            }
                            break;
                    }
                    beg += *newline + 1;
                } else if (end - beg >= max_line_len) {
                    parsing_invalid_message = true;
                } else {
                    break;
                }
            }
        }
        return {};
    }

    void TCPReceiveBuffer::populate() {
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
        if (res == 0)
            fatal("Iface closed connection!");
        end += res;
    }

    void UDPReceiveBuffer::verify_crc32(uint32_t len_before, uint32_t len_after) {
        if (pos + len_after + sizeof(crc32_t) > size)
            throw BadData{};
        crc32_t computed = Crc32Computer::compute_in_buffer(buff + pos - len_before,
                                                            len_before + len_after);
        crc32_t received = betoh(*reinterpret_cast<crc32_t*>(buff + pos + len_after));
        if (computed != received)
            throw Crc32Mismatch{};
    }

    std::string UDPReceiveBuffer::unpack_name() {
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

    bool UDPSendBuffer::flush() {
        ssize_t res;
        if (receiver.has_value())
            res = receiver->sendthere(buff, _size);
        else
            res = send(*receiver_sock, buff, _size, 0);
        if (res == -1) {
            if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
                syserr(errno, "cannot send to remote host (UDP)");
                return false;
            } else {
                return false;
            }
        } else {
            _size = 0;
            return true;
        }
    }
}

