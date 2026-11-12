#pragma once
// YOUR DNS CODEC: parse and serialize DNS messages, with name compression, safe against every malformed packet.
//
//   encode_name(name)          -> optional<vector<uint8_t>>   the UNCOMPRESSED wire form of a name: each label preceded by its length, then a zero byte.
//                                 Accepts "a.b.c", "a.b.c." and the root ("." or ""). nullopt for an empty label ("a..b", ".a"), a label longer than 63 bytes or containing a
//                                 character outside 0x21..0x7E or a '.', or a wire form longer than 255 bytes.
//   decode_name(msg, pos)      -> optional<string>   reads the name that starts at msg[pos], following compression pointers. On success `pos` is moved just past the name
//                                 AS IT APPEARS AT pos (past its terminating zero byte, or past its first pointer, NOT past whatever the pointer led to); on failure pos is unchanged.
//                                 The root name is returned as ".". Other names have no trailing dot.
//   parse(msg)                 -> optional<Message>   the whole message, or nullopt if it is malformed in ANY way (see below).
//   serialize(message)         -> optional<vector<uint8_t>>   the message with all names compressed; nullopt if something cannot be represented (invalid name or rdata,
//                                 more than 65535 entries in a section, a result longer than 65535 bytes).
//
// WIRE FORMAT: header of 6 big-endian 16-bit fields: id, flags, qdcount, ancount, nscount, arcount (12 bytes). Then the questions (name, type, class: 4 bytes after the name),
// then the answer, authority and additional records (name, type 2, class 2, ttl 4, rdlength 2, rdata). A name is a sequence of labels (a length byte 1..63, then that many bytes)
// ended by a zero byte, OR ended by a 2-byte compression pointer: the top two bits of the first byte are 11 and the other 14 bits are an offset from the start of the message
// where the rest of the name continues.
//
// DECODING RULES (nullopt for anything else):
//   * the top two bits of a length byte must be 00 or 11; 01 and 10 are reserved. Labels must fit in the message and consist of printable ASCII other than '.'.
//   * the expanded name must be at most 255 bytes on the wire (each label plus its length byte, plus the final zero).
//   * POINTERS MUST POINT BACKWARDS, to an offset >= 12 (not into the header), and each jump of one name must land strictly BEFORE the previous one (before the position where the
//     name started, for the first jump). That makes loops impossible. A pointer may lead to a name that itself ends in a pointer (a chain); it may not be followed recursively
//     without a bound: use a loop.
//   * records: rdlength bytes must be inside the message; A needs exactly 4, AAAA exactly 16; for NS/CNAME/PTR/MX the name inside rdata must end exactly where rdlength ends.
//   * the section counts are only CLAIMS: a 12-byte packet that claims 65535 questions must fail on the first missing byte without allocating anything for them.
//   * nothing may follow the last record.
//
// COMPRESSION when serializing: while writing a name, remember the offset of every suffix ("example.com", "com", ...) that starts at an offset below 0x4000 (a pointer cannot
// reach further); matching is case-INsensitive; when a suffix of the name being written is already known, write a pointer to it instead of the rest of the name. The first spelling
// of a suffix wins. Names inside the rdata of NS, CNAME, PTR and MX are compressed too. Write rdlength after the rdata is known.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace dns {

inline std::optional<std::vector<std::uint8_t>> encode_name(std::string_view name) {
    (void)name;
    return std::nullopt;  // TODO
}

inline std::optional<std::string> decode_name(std::span<const std::uint8_t> msg, std::size_t& pos) {
    (void)msg;
    (void)pos;
    return std::nullopt;  // TODO
}

inline std::optional<Message> parse(std::span<const std::uint8_t> msg) {
    (void)msg;
    return std::nullopt;  // TODO
}

inline std::optional<std::vector<std::uint8_t>> serialize(const Message& message) {
    (void)message;
    return std::nullopt;  // TODO
}

}  // namespace dns
