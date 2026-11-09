#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

// A tiny binary protocol, and its parser. THIS is the code under test: you fuzz it, you do not edit it.
//
// A message is a sequence of PACKETS. One packet is:
//
//     0xA5   N   payload (N bytes)   checksum
//     magic  length                  the sum of the payload bytes, modulo 256
//
// N is a single byte (0..255), so a payload has at most 255 bytes, and an EMPTY payload is allowed (the packet is then "A5 00 00").
//
//   make_packet(payload)       the bytes of one packet (std::length_error if the payload is longer than 255)
//   parse(data, size)          reads packets until the end of the message. result.ok is true only if the WHOLE input was valid;
//                              result.packets always holds the payloads that were read before the first problem. It must never crash,
//                              hang or read outside of [data, data + size), whatever the bytes are.

using Bytes = std::vector<std::uint8_t>;

struct ParseResult {
    std::vector<Bytes> packets;
    bool ok = false;
};

inline constexpr std::uint8_t kMagic = 0xA5;

inline Bytes make_packet(const Bytes& payload) {
    if (payload.size() > 255) throw std::length_error("payload longer than 255 bytes");
    Bytes packet;
    packet.push_back(kMagic);
    packet.push_back(static_cast<std::uint8_t>(payload.size()));
    std::uint8_t sum = 0;
    for (std::uint8_t b : payload) {
        packet.push_back(b);
        sum = static_cast<std::uint8_t>(sum + b);
    }
    packet.push_back(sum);
    return packet;
}

inline ParseResult parse(const std::uint8_t* data, std::size_t size) {
    ParseResult result;
    std::size_t pos = 0;
    while (pos < size) {
        if (data[pos] != kMagic) return result;  // not the start of a packet
        if (size - pos < 3) return result;       // no room for magic, length and checksum
        const std::size_t length = data[pos + 1];
        if (size - pos < 3 + length) return result;  // the packet is cut off
        std::uint8_t sum = 0;
        for (std::size_t i = 0; i < length; ++i) sum = static_cast<std::uint8_t>(sum + data[pos + 2 + i]);
        if (sum != data[pos + 2 + length]) return result;  // wrong checksum
        result.packets.emplace_back(data + pos + 2, data + pos + 2 + length);
        pos += 3 + length;
    }
    result.ok = true;
    return result;
}
