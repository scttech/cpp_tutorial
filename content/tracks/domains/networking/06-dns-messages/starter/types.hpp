#pragma once
// READ-ONLY. A DNS message (RFC 1035) in memory.
//
// Names are in text form without a trailing dot ("www.example.com"); the root name is ".". Labels are 1 to 63 printable ASCII characters (0x21 to 0x7E) other than '.'.
// The case of a name is kept as it was written.
//
// RDATA: for the types that contain a domain name, the rdata held here is ALWAYS the UNCOMPRESSED wire form, so that a record means the same thing wherever it is:
//     NS (2), CNAME (5), PTR (12) : the name in wire format (labels, each preceded by its length, ending with a zero byte)
//     MX (15)                     : a 2-byte big-endian preference, then the name in wire format
//     A (1): exactly 4 bytes. AAAA (28): exactly 16 bytes. Every other type: opaque bytes, kept as they are.
// parse() produces this form (it expands compression pointers inside rdata); serialize() accepts it and compresses the names again.
#include <cstdint>
#include <string>
#include <vector>

namespace dns {

struct Question {
    std::string name;
    std::uint16_t type = 0;
    std::uint16_t cls = 1;
    bool operator==(const Question&) const = default;
};

struct Record {
    std::string name;
    std::uint16_t type = 0;
    std::uint16_t cls = 1;
    std::uint32_t ttl = 0;
    std::vector<std::uint8_t> rdata;
    bool operator==(const Record&) const = default;
};

struct Message {
    std::uint16_t id = 0;
    std::uint16_t flags = 0;
    std::vector<Question> questions;
    std::vector<Record> answers;
    std::vector<Record> authorities;
    std::vector<Record> additionals;
    bool operator==(const Message&) const = default;
};

}  // namespace dns
