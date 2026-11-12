// A tiny binary protocol: varints, a length-prefixed string, and checksummed frames that may arrive in pieces. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "codec.hpp"
#include "types.hpp"

int main() {
    codec::ByteWriter w;
    w.put_u16be(0xCAFE);
    w.put_u32le(0x01020304);
    w.put_varint(300);
    w.put_svarint(-3);
    w.put_string("héllo");
    std::printf("encoded %zu bytes:", w.size());
    for (std::uint8_t b : w.bytes()) std::printf(" %02X", b);
    std::printf("\n");

    codec::ByteReader r(w.bytes());
    const auto a = r.get_u16be();
    const auto b = r.get_u32le();
    const auto c = r.get_varint();
    const auto d = r.get_svarint();
    const auto s = r.get_string();
    std::printf("decoded: %04X %08X %llu %lld \"%s\"  (%zu bytes left)\n", *a, *b, static_cast<unsigned long long>(*c), static_cast<long long>(*d), std::string(*s).c_str(), r.remaining());
    std::printf("reading past the end: %s\n", r.get_u8() ? "got a byte?!" : "nullopt, nothing moved");

    const std::vector<std::uint8_t> payload = {'p', 'i', 'n', 'g'};
    const std::vector<std::uint8_t> wire = codec::encode_frame(7, payload);
    codec::FrameDecoder dec;
    for (std::uint8_t byte : wire) dec.feed(std::span<const std::uint8_t>(&byte, 1));   // one byte at a time
    if (auto frame = dec.next()) std::printf("frame type %u with %zu payload bytes arrived\n", frame->type, frame->payload.size());
}
