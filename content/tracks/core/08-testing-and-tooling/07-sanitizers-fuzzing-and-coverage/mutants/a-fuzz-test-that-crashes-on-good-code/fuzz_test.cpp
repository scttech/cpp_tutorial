// MUTANT: the fuzz test itself must survive the real parser.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "packet.hpp"

namespace {

std::mt19937 rng(12345);  // a FIXED seed: any failure can be reproduced exactly

std::uint8_t random_byte() { return static_cast<std::uint8_t>(rng()); }

Bytes random_bytes(std::size_t max_size) {
    Bytes bytes(rng() % (max_size + 1));
    for (auto& b : bytes) b = random_byte();
    return bytes;
}

// parse() on an EXACTLY sized copy: the vector's buffer ends where the input ends, so AddressSanitizer notices every read past the last byte.
ParseResult parse_exact(const Bytes& input) {
    const Bytes copy = input;
    return parse(copy.data(), copy.size());
}

[[noreturn]] void fail(const char* what) {
    std::fprintf(stderr, "property violated: %s\n", what);
    std::abort();
}

Bytes encode(const std::vector<Bytes>& payloads) {
    Bytes message;
    for (const Bytes& p : payloads) {
        const Bytes packet = make_packet(p);
        message.insert(message.end(), packet.begin(), packet.end());
    }
    return message;
}

std::vector<Bytes> random_payloads(std::size_t max_packets, std::size_t max_payload) {
    std::vector<Bytes> payloads(rng() % (max_packets + 1));
    for (Bytes& p : payloads) p = random_bytes(max_payload);
    return payloads;
}

// 1. Random bytes: most are rejected at the first byte, but some get further.
void fuzz_random_bytes() {
    for (int i = 0; i < 20000; ++i) parse_exact(random_bytes(12));
    std::abort();
}

// 2. Valid messages (including EMPTY payloads) must come back exactly as they were encoded: the round-trip property.
void fuzz_round_trip() {
    for (int i = 0; i < 2000; ++i) {
        const std::vector<Bytes> payloads = random_payloads(4, 8);
        const ParseResult result = parse_exact(encode(payloads));
        if (!result.ok) fail("a valid message was rejected");
        if (result.packets != payloads) fail("the packets that came back differ from the ones that went in");
    }
    const ParseResult empty = parse_exact(make_packet({}));  // the smallest packet: A5 00 00
    if (!empty.ok || empty.packets.size() != 1) fail("an empty packet was lost");
}

// 3. Payloads at the top of the range (the length byte is >= 128).
void fuzz_long_payloads() {
    for (const std::size_t length : {127u, 128u, 200u, 255u}) {
        const std::vector<Bytes> payloads = {Bytes(length, 7)};
        const ParseResult result = parse_exact(encode(payloads));
        if (!result.ok || result.packets != payloads) fail("a long payload did not round-trip");
    }
}

// 4. Every prefix of a valid message is a message that was cut off in the middle of something: it must be rejected, never crash.
void fuzz_every_prefix() {
    for (int i = 0; i < 300; ++i) {
        std::vector<Bytes> payloads = random_payloads(3, 6);
        payloads.push_back(Bytes{1, 2, 3});  // at least one real packet
        const Bytes message = encode(payloads);
        for (std::size_t cut = 0; cut < message.size(); ++cut) {
            parse_exact(Bytes(message.begin(), message.begin() + static_cast<std::ptrdiff_t>(cut)));
        }
    }
}

// 5. A valid message with one byte changed: usually a wrong checksum, sometimes a wrong magic or length.
void fuzz_flipped_bytes() {
    for (int i = 0; i < 3000; ++i) {
        Bytes message = encode(random_payloads(3, 6));
        if (message.empty()) continue;
        message[rng() % message.size()] = random_byte();
        parse_exact(message);
    }
}

}  // namespace

int main() {
    fuzz_random_bytes();
    fuzz_round_trip();
    fuzz_long_payloads();
    fuzz_every_prefix();
    fuzz_flipped_bytes();
    std::puts("fuzzing finished");
    return 0;
}
