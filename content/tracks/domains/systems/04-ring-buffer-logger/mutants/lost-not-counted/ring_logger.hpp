// MUTANT: logs that cannot be stored are not counted as lost
#pragma once
// One flat byte buffer used as a circle: records [level][seq][length][message] end to end, oldest at head_, `used_` bytes in use.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace flightlog {

class RingLogger {
public:
    static constexpr std::size_t kHeaderBytes = 13;

    explicit RingLogger(std::size_t capacity_bytes) : buf_(capacity_bytes) {}

    void set_min_level(Level level) { min_level_ = level; }
    Level min_level() const { return min_level_; }

    bool log(Level level, std::string_view message) {
        if (level < min_level_) {
            ++filtered_;
            return false;
        }
        const std::uint64_t seq = ++total_logged_;
        if (buf_.size() <= kHeaderBytes) {
            return false;
        }
        std::size_t length = message.size();
        if (kHeaderBytes + length > buf_.size()) {
            length = buf_.size() - kHeaderBytes;
            ++truncated_;
        }
        const std::size_t needed = kHeaderBytes + length;
        while (buf_.size() - used_ < needed) drop_oldest();

        char header[kHeaderBytes];
        header[0] = static_cast<char>(level);
        for (std::size_t i = 0; i < 8; ++i) header[1 + i] = static_cast<char>((seq >> (8 * i)) & 0xFF);
        for (std::size_t i = 0; i < 4; ++i) header[9 + i] = static_cast<char>((length >> (8 * i)) & 0xFF);
        std::size_t pos = (head_ + used_) % buf_.size();
        pos = write(pos, header, kHeaderBytes);
        write(pos, message.data(), length);
        used_ += needed;
        ++count_;
        return true;
    }

    std::size_t size() const { return count_; }
    std::size_t bytes_used() const { return used_; }
    std::size_t capacity() const { return buf_.size(); }
    std::uint64_t total_logged() const { return total_logged_; }
    std::uint64_t evicted() const { return evicted_; }
    std::uint64_t truncated() const { return truncated_; }
    std::uint64_t filtered() const { return filtered_; }
    std::uint64_t lost() const { return lost_; }

    std::vector<Entry> snapshot() const {
        std::vector<Entry> out;
        out.reserve(count_);
        std::size_t pos = head_;
        for (std::size_t k = 0; k < count_; ++k) {
            char header[kHeaderBytes];
            read(pos, header, kHeaderBytes);
            Entry e;
            e.level = static_cast<Level>(static_cast<unsigned char>(header[0]));
            for (std::size_t i = 0; i < 8; ++i) e.seq |= static_cast<std::uint64_t>(static_cast<unsigned char>(header[1 + i])) << (8 * i);
            const std::size_t length = decode_length(header);
            e.message.resize(length);
            read((pos + kHeaderBytes) % buf_.size(), e.message.data(), length);
            pos = (pos + kHeaderBytes + length) % buf_.size();
            out.push_back(std::move(e));
        }
        return out;
    }

    std::string dump() const {
        std::string text;
        for (const Entry& e : snapshot()) {
            text += "#" + std::to_string(e.seq) + " " + level_name(e.level) + " " + e.message + "\n";
        }
        return text;
    }

    void clear() {
        head_ = 0;
        used_ = 0;
        count_ = 0;
    }

private:
    static std::size_t decode_length(const char* header) {
        std::size_t length = 0;
        for (std::size_t i = 0; i < 4; ++i) length |= static_cast<std::size_t>(static_cast<unsigned char>(header[9 + i])) << (8 * i);
        return length;
    }

    // Copy n bytes into the circle starting at pos, wrapping at the end. Returns the position after them.
    std::size_t write(std::size_t pos, const char* data, std::size_t n) {
        const std::size_t first = std::min(n, buf_.size() - pos);
        std::copy(data, data + first, buf_.begin() + static_cast<std::ptrdiff_t>(pos));
        std::copy(data + first, data + n, buf_.begin());
        return (pos + n) % buf_.size();
    }

    // Copy n bytes out of the circle starting at pos, wrapping at the end.
    void read(std::size_t pos, char* out, std::size_t n) const {
        const std::size_t first = std::min(n, buf_.size() - pos);
        std::copy(buf_.begin() + static_cast<std::ptrdiff_t>(pos), buf_.begin() + static_cast<std::ptrdiff_t>(pos + first), out);
        std::copy(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(n - first), out + first);
    }

    void drop_oldest() {
        char header[kHeaderBytes];
        read(head_, header, kHeaderBytes);
        const std::size_t size = kHeaderBytes + decode_length(header);
        head_ = (head_ + size) % buf_.size();
        used_ -= size;
        --count_;
        ++evicted_;
    }

    std::vector<char> buf_;
    std::size_t head_ = 0;   // offset of the oldest record
    std::size_t used_ = 0;   // bytes occupied by records
    std::size_t count_ = 0;  // records stored
    Level min_level_ = Level::Debug;
    std::uint64_t total_logged_ = 0;
    std::uint64_t evicted_ = 0;
    std::uint64_t truncated_ = 0;
    std::uint64_t filtered_ = 0;
    std::uint64_t lost_ = 0;
};

}  // namespace flightlog
