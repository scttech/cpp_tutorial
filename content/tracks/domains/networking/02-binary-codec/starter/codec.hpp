#pragma once
// YOUR BINARY CODEC: write and read the primitive types of a network protocol, and decode checksummed frames from a byte stream.
//
// ByteWriter appends to an internal std::vector<std::uint8_t>:
//   put_u8(v)  put_u16be/u32be/u64be(v)  put_u16le/u32le/u64le(v)    fixed-size integers, most (be) or least (le) significant byte first, on ANY host
//   put_varint(u64)     LEB128: 7 bits per byte, least significant group first, bit 7 set on every byte except the last. 0..127 take one byte.
//   put_svarint(i64)    zigzag first (0, -1, 1, -2, 2 ... become 0, 1, 2, 3, 4 ...: (n << 1) ^ (n >> 63)), then put_varint
//   put_bytes(span)     raw bytes                      put_string(string_view)   varint length, then the bytes
//   size(), bytes() (const ref), clear(), reserve(n)
//
// ByteReader reads from a span without copying. EVERY getter returns std::optional and, when the data is too short or invalid, returns nullopt
// and leaves the position EXACTLY where it was (a failed read consumes nothing):
//   get_u8, get_u16be/u32be/u64be, get_u16le/u32le/u64le, get_varint, get_svarint
//   get_varint is STRICT: at most 10 bytes; the 10th byte may only be 0 or 1 (more would overflow 64 bits); no redundant zero byte
//                (a last byte of 0 after an earlier byte is a non-canonical encoding, e.g. {0x80, 0x00} for 0, and is refused).
//   get_bytes(n)        a sub-span of exactly n bytes      get_string()   varint length then that many bytes as a string_view
//                       (a length larger than what is left fails at once; nothing is allocated or copied)
//   position(), remaining(), at_end()
//
// encode_frame(type, payload) -> the bytes of a frame (see types.hpp).
// FrameDecoder(max_payload = 65536): an incremental decoder for a STREAM of frames.
//   size_t feed(span)   consumes bytes one at a time (any split of the stream gives the same frames); returns how many it consumed:
//                       all of them, unless an error occurred (it stops at the bad byte). After an error it consumes nothing until reset().
//   optional<Frame> next()   the oldest decoded frame, or nullopt
//   error(), reset()    buffered_bytes(): bytes of the frame under construction that are stored (payload so far)
//   Errors (types.hpp): BadVersion at the first byte; BadLength / TooLarge as soon as the length is complete (TooLarge BEFORE any payload is stored);
//   BadChecksum at the last byte.
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
    void put_u8(std::uint8_t v) { (void)v; }  // TODO
    void put_u16be(std::uint16_t v) { (void)v; }
    void put_u32be(std::uint32_t v) { (void)v; }
    void put_u64be(std::uint64_t v) { (void)v; }
    void put_u16le(std::uint16_t v) { (void)v; }
    void put_u32le(std::uint32_t v) { (void)v; }
    void put_u64le(std::uint64_t v) { (void)v; }
    void put_varint(std::uint64_t v) { (void)v; }
    void put_svarint(std::int64_t v) { (void)v; }
    void put_bytes(std::span<const std::uint8_t> data) { (void)data; }
    void put_string(std::string_view text) { (void)text; }
    std::size_t size() const { return bytes_.size(); }
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    void clear() { bytes_.clear(); }
    void reserve(std::size_t n) { bytes_.reserve(n); }

private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data) {}
    std::optional<std::uint8_t> get_u8() { return std::nullopt; }  // TODO
    std::optional<std::uint16_t> get_u16be() { return std::nullopt; }
    std::optional<std::uint32_t> get_u32be() { return std::nullopt; }
    std::optional<std::uint64_t> get_u64be() { return std::nullopt; }
    std::optional<std::uint16_t> get_u16le() { return std::nullopt; }
    std::optional<std::uint32_t> get_u32le() { return std::nullopt; }
    std::optional<std::uint64_t> get_u64le() { return std::nullopt; }
    std::optional<std::uint64_t> get_varint() { return std::nullopt; }
    std::optional<std::int64_t> get_svarint() { return std::nullopt; }
    std::optional<std::span<const std::uint8_t>> get_bytes(std::size_t n) {
        (void)n;
        return std::nullopt;
    }
    std::optional<std::string_view> get_string() { return std::nullopt; }
    std::size_t position() const { return pos_; }
    std::size_t remaining() const { return data_.size() - pos_; }
    bool at_end() const { return pos_ == data_.size(); }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

inline std::vector<std::uint8_t> encode_frame(std::uint8_t type, std::span<const std::uint8_t> payload) {
    (void)type;
    (void)payload;
    return {};  // TODO
}

class FrameDecoder {
public:
    explicit FrameDecoder(std::size_t max_payload = 65536) : max_payload_(max_payload) {}
    std::size_t feed(std::span<const std::uint8_t> data) {
        (void)data;
        return 0;  // TODO
    }
    std::optional<Frame> next() { return std::nullopt; }  // TODO
    FrameError error() const { return FrameError::None; }  // TODO
    std::size_t max_payload() const { return max_payload_; }
    std::size_t buffered_bytes() const { return 0; }       // TODO
    void reset() {}                                         // TODO

private:
    std::size_t max_payload_;
    // TODO: the state machine
};

}  // namespace codec
