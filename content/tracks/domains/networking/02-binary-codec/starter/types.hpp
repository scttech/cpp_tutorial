#pragma once
// READ-ONLY. The frame format and the vocabulary of the codec.
//
//   FRAME (every integer is in the formats of codec.hpp):
//     +---------+------+----------------------+-----------------+----------+
//     | version | type | payload length       | payload         | checksum |
//     | 1 byte  | 1 B  | unsigned varint      | `length` bytes  | 1 byte   |
//     +---------+------+----------------------+-----------------+----------+
//   version is always 1. checksum = 0x5A XOR (every byte of the frame before it: version, type, the length bytes, the payload).
#include <cstdint>
#include <vector>

namespace codec {

inline constexpr std::uint8_t kFrameVersion = 1;
inline constexpr std::uint8_t kChecksumSeed = 0x5A;

struct Frame {
    std::uint8_t type = 0;
    std::vector<std::uint8_t> payload;
};

enum class FrameError {
    None,
    BadVersion,   // the first byte of a frame is not 1
    BadLength,    // the payload length is not a valid canonical varint (too long, overflowing, or with a redundant zero byte)
    TooLarge,     // the payload length exceeds max_payload (detected as soon as the length is known, before any payload is stored)
    BadChecksum,  // the checksum byte does not match
};

}  // namespace codec
