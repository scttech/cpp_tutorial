// MUTANT: zigzag decoding does not restore the sign
#pragma once
// Fixed-size integers by shifts (host-independent), LEB128 varints, a span-based reader whose failed reads move nothing, and a byte-wise frame decoder.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace codec {

class ByteWriter {
public:
    void put_u8(std::uint8_t v) { bytes_.push_back(v); }
    void put_u16be(std::uint16_t v) { put_be(v, 2); }
    void put_u32be(std::uint32_t v) { put_be(v, 4); }
    void put_u64be(std::uint64_t v) { put_be(v, 8); }
    void put_u16le(std::uint16_t v) { put_le(v, 2); }
    void put_u32le(std::uint32_t v) { put_le(v, 4); }
    void put_u64le(std::uint64_t v) { put_le(v, 8); }

    void put_varint(std::uint64_t v) {
        while (v >= 0x80) {
            bytes_.push_back(static_cast<std::uint8_t>((v & 0x7F) | 0x80));
            v >>= 7;
        }
        bytes_.push_back(static_cast<std::uint8_t>(v));
    }
    void put_svarint(std::int64_t v) { put_varint((static_cast<std::uint64_t>(v) << 1) ^ static_cast<std::uint64_t>(v >> 63)); }

    void put_bytes(std::span<const std::uint8_t> data) { bytes_.insert(bytes_.end(), data.begin(), data.end()); }
    void put_string(std::string_view text) {
        put_varint(text.size());
        bytes_.insert(bytes_.end(), text.begin(), text.end());
    }

    std::size_t size() const { return bytes_.size(); }
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    void clear() { bytes_.clear(); }
    void reserve(std::size_t n) { bytes_.reserve(n); }

private:
    void put_be(std::uint64_t v, int n) {
        for (int i = n - 1; i >= 0; --i) bytes_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
    void put_le(std::uint64_t v, int n) {
        for (int i = 0; i < n; ++i) bytes_.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}

    std::optional<std::uint8_t> get_u8() {
        if (remaining() < 1) return std::nullopt;
        return data_[pos_++];
    }
    std::optional<std::uint16_t> get_u16be() { return narrow16(fixed(2, true)); }
    std::optional<std::uint32_t> get_u32be() { return narrow32(fixed(4, true)); }
    std::optional<std::uint64_t> get_u64be() { return fixed(8, true); }
    std::optional<std::uint16_t> get_u16le() { return narrow16(fixed(2, false)); }
    std::optional<std::uint32_t> get_u32le() { return narrow32(fixed(4, false)); }
    std::optional<std::uint64_t> get_u64le() { return fixed(8, false); }

    std::optional<std::uint64_t> get_varint() {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < 10; ++i) {
            if (pos_ + i >= data_.size()) return std::nullopt;   // truncated
            const std::uint8_t b = data_[pos_ + i];
            if (i == 9 && b > 1) return std::nullopt;            // would not fit in 64 bits
            value |= static_cast<std::uint64_t>(b & 0x7F) << (7 * i);
            if ((b & 0x80) == 0) {
                if (i > 0 && b == 0) return std::nullopt;        // redundant zero byte: not canonical
                pos_ += i + 1;
                return value;
            }
        }
        return std::nullopt;                                     // 10 bytes and the last one still says "more"
    }
    std::optional<std::int64_t> get_svarint() {
        const auto u = get_varint();
        if (!u) return std::nullopt;
        return static_cast<std::int64_t>(*u >> 1);
    }

    std::optional<std::span<const std::uint8_t>> get_bytes(std::size_t n) {
        if (n > remaining()) return std::nullopt;
        const auto s = data_.subspan(pos_, n);
        pos_ += n;
        return s;
    }
    std::optional<std::string_view> get_string() {
        const std::size_t start = pos_;
        const auto len = get_varint();
        if (!len || *len > remaining()) {
            pos_ = start;
            return std::nullopt;
        }
        const auto s = data_.subspan(pos_, static_cast<std::size_t>(*len));
        pos_ += static_cast<std::size_t>(*len);
        return std::string_view(reinterpret_cast<const char*>(s.data()), s.size());
    }

    std::size_t position() const { return pos_; }
    std::size_t remaining() const { return data_.size() - pos_; }
    bool at_end() const { return pos_ == data_.size(); }

private:
    std::optional<std::uint64_t> fixed(std::size_t n, bool big_endian) {
        if (remaining() < n) return std::nullopt;
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t b = data_[pos_ + i];
            v |= big_endian ? b << (8 * (n - 1 - i)) : b << (8 * i);
        }
        pos_ += n;
        return v;
    }
    static std::optional<std::uint16_t> narrow16(std::optional<std::uint64_t> v) { return v ? std::optional<std::uint16_t>(static_cast<std::uint16_t>(*v)) : std::nullopt; }
    static std::optional<std::uint32_t> narrow32(std::optional<std::uint64_t> v) { return v ? std::optional<std::uint32_t>(static_cast<std::uint32_t>(*v)) : std::nullopt; }

    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

inline std::vector<std::uint8_t> encode_frame(std::uint8_t type, std::span<const std::uint8_t> payload) {
    ByteWriter w;
    w.reserve(payload.size() + 14);
    w.put_u8(kFrameVersion);
    w.put_u8(type);
    w.put_varint(payload.size());
    w.put_bytes(payload);
    std::uint8_t sum = kChecksumSeed;
    for (std::uint8_t b : w.bytes()) sum = static_cast<std::uint8_t>(sum ^ b);
    w.put_u8(sum);
    return w.bytes();
}

class FrameDecoder {
public:
    explicit FrameDecoder(std::size_t max_payload = 65536) : max_payload_(max_payload) {}

    std::size_t feed(std::span<const std::uint8_t> data) {
        std::size_t used = 0;
        for (std::uint8_t b : data) {
            if (error_ != FrameError::None) break;
            ++used;
            consume(b);
        }
        return used;   // the bad byte counts as consumed; the bytes after it are not read
    }

    std::optional<Frame> next() {
        if (done_.empty()) return std::nullopt;
        Frame f = std::move(done_.front());
        done_.pop_front();
        return f;
    }

    FrameError error() const { return error_; }
    std::size_t buffered_bytes() const { return current_.payload.size(); }

    void reset() {
        error_ = FrameError::None;
        stage_ = Stage::Version;
        current_ = Frame{};
        done_.clear();
    }

private:
    enum class Stage { Version, Type, Length, Payload, Checksum };

    void fail(FrameError e) { error_ = e; }

    void consume(std::uint8_t b) {
        if (stage_ != Stage::Checksum) sum_ = static_cast<std::uint8_t>(stage_ == Stage::Version ? (kChecksumSeed ^ b) : (sum_ ^ b));
        switch (stage_) {
            case Stage::Version:
                if (b != kFrameVersion) return fail(FrameError::BadVersion);
                stage_ = Stage::Type;
                break;
            case Stage::Type:
                current_ = Frame{};
                current_.type = b;
                length_ = 0;
                length_bytes_ = 0;
                stage_ = Stage::Length;
                break;
            case Stage::Length: {
                if (length_bytes_ == 9 && b > 1) return fail(FrameError::BadLength);
                length_ |= static_cast<std::uint64_t>(b & 0x7F) << (7 * length_bytes_);
                ++length_bytes_;
                if (b & 0x80) {
                    if (length_bytes_ == 10) fail(FrameError::BadLength);
                    break;
                }
                if (length_bytes_ > 1 && b == 0) return fail(FrameError::BadLength);
                if (length_ > max_payload_) return fail(FrameError::TooLarge);
                current_.payload.reserve(static_cast<std::size_t>(length_) < 4096 ? static_cast<std::size_t>(length_) : 4096);
                stage_ = length_ == 0 ? Stage::Checksum : Stage::Payload;
                break;
            }
            case Stage::Payload:
                current_.payload.push_back(b);
                if (current_.payload.size() == length_) stage_ = Stage::Checksum;
                break;
            case Stage::Checksum:
                if (b != sum_) return fail(FrameError::BadChecksum);
                done_.push_back(std::move(current_));
                current_ = Frame{};
                stage_ = Stage::Version;
                break;
        }
    }

    std::size_t max_payload_;
    FrameError error_ = FrameError::None;
    Stage stage_ = Stage::Version;
    Frame current_;
    std::uint64_t length_ = 0;
    std::size_t length_bytes_ = 0;
    std::uint8_t sum_ = 0;
    std::deque<Frame> done_;
};

}  // namespace codec
