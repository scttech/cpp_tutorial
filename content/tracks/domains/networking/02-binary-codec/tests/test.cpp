#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "codec.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace codec;
using Bytes = std::vector<std::uint8_t>;

namespace {

std::string hex(const Bytes& b) {
    static const char* d = "0123456789ABCDEF";
    std::string s;
    for (std::uint8_t x : b) {
        s += d[x >> 4];
        s += d[x & 15];
        s += ' ';
    }
    return s;
}
Bytes bytes_of(std::initializer_list<int> v) {
    Bytes b;
    for (int x : v) b.push_back(static_cast<std::uint8_t>(x));
    return b;
}
std::span<const std::uint8_t> sp(const Bytes& b) { return std::span<const std::uint8_t>(b.data(), b.size()); }
template <class T>
T val(const std::optional<T>& o) {
    return o.value_or(T{});
}
Bytes varint(std::uint64_t v) {
    ByteWriter w;
    w.put_varint(v);
    return w.bytes();
}

struct Var {
    std::uint64_t value;
    Bytes bytes;
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "fixed-size integers are written in the byte order that was asked for, whatever the host") {
    ByteWriter w;
    w.put_u8(0xAB);
    w.put_u16be(0x0102);
    w.put_u32be(0x01020304);
    w.put_u64be(0x0102030405060708ULL);
    w.put_u16le(0x0102);
    w.put_u32le(0x01020304);
    w.put_u64le(0x0102030405060708ULL);
    ZTH_EXPECT_EQ(hex(w.bytes()), std::string("AB 01 02 01 02 03 04 01 02 03 04 05 06 07 08 02 01 04 03 02 01 08 07 06 05 04 03 02 01 "));
    ZTH_EXPECT_EQ(w.size(), std::size_t{29});
}
ZTH_TEST("works", "the extreme values of every width") {
    ByteWriter w;
    w.put_u16be(0xFFFF);
    w.put_u32le(0x80000000u);
    w.put_u64be(0x8000000000000001ULL);
    w.put_u16le(0);
    ZTH_EXPECT_EQ(hex(w.bytes()), std::string("FF FF 00 00 00 80 80 00 00 00 00 00 00 01 00 00 "));
}
ZTH_TEST("works", "varints: the well-known encodings") {
    const Var cases[] = {
        {0, {0x00}}, {1, {0x01}}, {127, {0x7F}}, {128, {0x80, 0x01}}, {300, {0xAC, 0x02}}, {16383, {0xFF, 0x7F}}, {16384, {0x80, 0x80, 0x01}},
        {1ULL << 32, {0x80, 0x80, 0x80, 0x80, 0x10}}, {1ULL << 63, {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}},
        {std::numeric_limits<std::uint64_t>::max(), {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}},
    };
    for (const Var& c : cases) {
        const Bytes want = c.bytes;
        if (varint(c.value) != want) {
            zth::fail("varint " + std::to_string(c.value) + " encodes as " + hex(varint(c.value)) + ", expected " + hex(want));
            return;
        }
        ByteReader r(sp(want));
        const auto back = r.get_varint();
        if (!back || *back != c.value || !r.at_end()) {
            zth::fail("varint " + hex(want) + " does not decode to " + std::to_string(c.value));
            return;
        }
    }
}
ZTH_TEST("works", "zigzag: small magnitudes of either sign become small varints") {
    struct Z {
        std::int64_t v;
        std::uint64_t zig;
    };
    const Z cases[] = {{0, 0}, {-1, 1}, {1, 2}, {-2, 3}, {2, 4}, {63, 126}, {-64, 127}, {64, 128}, {2147483647LL, 4294967294ULL}, {-2147483648LL, 4294967295ULL},
                       {std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::uint64_t>::max() - 1}, {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::uint64_t>::max()}};
    for (const Z& c : cases) {
        ByteWriter w;
        w.put_svarint(c.v);
        if (w.bytes() != varint(c.zig)) {
            zth::fail("svarint " + std::to_string(c.v) + " encodes as " + hex(w.bytes()) + ", expected the varint of " + std::to_string(c.zig));
            return;
        }
        ByteReader r(sp(w.bytes()));
        const auto back = r.get_svarint();
        if (!back || *back != c.v) {
            zth::fail("svarint " + std::to_string(c.v) + " does not round-trip");
            return;
        }
    }
    ByteWriter w;
    w.put_svarint(-1);
    ZTH_EXPECT_EQ(w.size(), std::size_t{1});   // -1 takes one byte, not ten
}
ZTH_TEST("works", "reading back what was written") {
    ByteWriter w;
    w.put_u8(7);
    w.put_u16be(0xBEEF);
    w.put_u32le(0xDEADBEEF);
    w.put_u64be(0x1122334455667788ULL);
    w.put_varint(123456789);
    w.put_svarint(-98765);
    w.put_string("héllo wörld");
    w.put_bytes(sp(bytes_of({1, 2, 3})));
    ByteReader r(sp(w.bytes()));
    ZTH_EXPECT_EQ(static_cast<int>(val(r.get_u8())), 7);
    ZTH_EXPECT_EQ(static_cast<unsigned>(val(r.get_u16be())), 0xBEEFu);
    ZTH_EXPECT_EQ(val(r.get_u32le()), std::uint32_t{0xDEADBEEF});
    ZTH_EXPECT_EQ(val(r.get_u64be()), std::uint64_t{0x1122334455667788ULL});
    ZTH_EXPECT_EQ(val(r.get_varint()), std::uint64_t{123456789});
    ZTH_EXPECT_EQ(val(r.get_svarint()), std::int64_t{-98765});
    ZTH_EXPECT_EQ(std::string(val(r.get_string())), std::string("héllo wörld"));
    const auto tail = r.get_bytes(3);
    ZTH_EXPECT_TRUE(tail.has_value() && tail->size() == 3 && (*tail)[2] == 3);
    ZTH_EXPECT_TRUE(r.at_end());
    ZTH_EXPECT_EQ(r.remaining(), std::size_t{0});
}
ZTH_TEST("works", "the other orders: little-endian 16/64 bits and big-endian 16/32/64 read correctly") {
    const Bytes b = bytes_of({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    ZTH_EXPECT_EQ(static_cast<unsigned>(val(ByteReader(sp(b)).get_u16be())), 0x0102u);
    ZTH_EXPECT_EQ(static_cast<unsigned>(val(ByteReader(sp(b)).get_u16le())), 0x0201u);
    ZTH_EXPECT_EQ(val(ByteReader(sp(b)).get_u32be()), std::uint32_t{0x01020304});
    ZTH_EXPECT_EQ(val(ByteReader(sp(b)).get_u64be()), std::uint64_t{0x0102030405060708ULL});
    ZTH_EXPECT_EQ(val(ByteReader(sp(b)).get_u64le()), std::uint64_t{0x0807060504030201ULL});
}
ZTH_TEST("works", "a read past the end returns nullopt and moves nothing") {
    const Bytes b = bytes_of({1, 2, 3});
    ByteReader r(sp(b));
    ZTH_EXPECT_FALSE(r.get_u32be().has_value());
    ZTH_EXPECT_FALSE(r.get_u64le().has_value());
    ZTH_EXPECT_EQ(r.position(), std::size_t{0});
    ZTH_EXPECT_TRUE(r.get_u16be().has_value());
    ZTH_EXPECT_EQ(r.position(), std::size_t{2});
    ZTH_EXPECT_FALSE(r.get_u16le().has_value());     // only one byte left
    ZTH_EXPECT_EQ(r.position(), std::size_t{2});
    ZTH_EXPECT_TRUE(r.get_u8().has_value());
    ZTH_EXPECT_FALSE(r.get_u8().has_value());
    ZTH_EXPECT_FALSE(r.get_varint().has_value());
    ZTH_EXPECT_FALSE(r.get_bytes(1).has_value());
    ZTH_EXPECT_TRUE(r.get_bytes(0).has_value());
    ZTH_EXPECT_FALSE(r.get_string().has_value());
    ZTH_EXPECT_EQ(r.position(), std::size_t{3});
}
ZTH_TEST("works", "a truncated varint fails and consumes nothing") {
    const Bytes b = bytes_of({0x80, 0x80});
    ByteReader r(sp(b));
    ZTH_EXPECT_FALSE(r.get_varint().has_value());
    ZTH_EXPECT_EQ(r.position(), std::size_t{0});
    ZTH_EXPECT_FALSE(r.get_svarint().has_value());
    ZTH_EXPECT_EQ(r.position(), std::size_t{0});
}
ZTH_TEST("works", "varints are strict: non-canonical and overflowing encodings are refused, and refusing consumes nothing") {
    const std::vector<Bytes> bad = {
        bytes_of({0x80, 0x00}),                                                       // 0 written in two bytes
        bytes_of({0xFF, 0x00}),                                                       // 127 written in two bytes
        bytes_of({0x80, 0x80, 0x00}),
        bytes_of({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}),       // 2^64 + ...: does not fit
        bytes_of({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}),
        bytes_of({0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}),   // 11 bytes
        bytes_of({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}),        // 10 bytes and still "more"
    };
    for (const Bytes& b : bad) {
        ByteReader r(sp(b));
        if (r.get_varint().has_value() || r.position() != 0) {
            zth::fail("the varint " + hex(b) + "should have been refused");
            return;
        }
    }
    const Bytes top_bit = bytes_of({0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01});
    ByteReader ok(sp(top_bit));
    ZTH_EXPECT_EQ(val(ok.get_varint()), std::uint64_t{1} << 63);
}
ZTH_TEST("works", "a varint stops at its own last byte: the following bytes are untouched") {
    const Bytes b = bytes_of({0xAC, 0x02, 0xAA, 0xBB});
    ByteReader r(sp(b));
    ZTH_EXPECT_EQ(val(r.get_varint()), std::uint64_t{300});
    ZTH_EXPECT_EQ(r.position(), std::size_t{2});
    ZTH_EXPECT_EQ(static_cast<int>(val(r.get_u8())), 0xAA);
}
ZTH_TEST("works", "strings: length prefix, content, and a length that promises more than is there") {
    ByteWriter w;
    w.put_string("");
    w.put_string("abc");
    ZTH_EXPECT_EQ(hex(w.bytes()), std::string("00 03 61 62 63 "));
    ByteReader r(sp(w.bytes()));
    ZTH_EXPECT_EQ(val(r.get_string()).size(), std::size_t{0});
    ZTH_EXPECT_EQ(std::string(val(r.get_string())), std::string("abc"));
    const Bytes lie = bytes_of({0x05, 'a', 'b'});
    ByteReader q(sp(lie));
    ZTH_EXPECT_FALSE(q.get_string().has_value());
    ZTH_EXPECT_EQ(q.position(), std::size_t{0});
    const Bytes huge = bytes_of({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 'x'});
    ByteReader h(sp(huge));
    ZTH_EXPECT_FALSE(h.get_string().has_value());
    ZTH_EXPECT_EQ(h.position(), std::size_t{0});
}
ZTH_TEST("works", "a string of 300 bytes has a two-byte length prefix") {
    const std::string big(300, 'q');
    ByteWriter w;
    w.put_string(big);
    ZTH_EXPECT_EQ(w.size(), std::size_t{302});
    ZTH_EXPECT_EQ(static_cast<int>(w.bytes()[0]), 0xAC);
    ZTH_EXPECT_EQ(static_cast<int>(w.bytes()[1]), 0x02);
    ByteReader r(sp(w.bytes()));
    ZTH_EXPECT_TRUE(val(r.get_string()) == std::string_view(big));
}
ZTH_TEST("works", "the string_view points into the original buffer: no copy") {
    ByteWriter w;
    w.put_string("zero copy");
    ByteReader r(sp(w.bytes()));
    const auto s = r.get_string();
    ZTH_EXPECT_TRUE(s.has_value());
    ZTH_EXPECT_TRUE(reinterpret_cast<const std::uint8_t*>(s->data()) == w.bytes().data() + 1);
    const Bytes data = bytes_of({1, 2, 3, 4});
    ByteReader q(sp(data));
    ZTH_EXPECT_TRUE(q.get_bytes(2)->data() == data.data());
}
ZTH_TEST("works", "encode_frame produces version, type, length, payload and the XOR checksum") {
    const Bytes payload = bytes_of({0x10, 0x20, 0x30});
    // checksum: 5A ^ 01 ^ 07 ^ 03 ^ 10 ^ 20 ^ 30
    ZTH_EXPECT_EQ(hex(encode_frame(7, sp(payload))), std::string("01 07 03 10 20 30 ") + hex(Bytes{static_cast<std::uint8_t>(0x5A ^ 0x01 ^ 0x07 ^ 0x03 ^ 0x10 ^ 0x20 ^ 0x30)}));
    ZTH_EXPECT_EQ(hex(encode_frame(0, sp(Bytes{}))), std::string("01 00 00 ") + hex(Bytes{static_cast<std::uint8_t>(0x5A ^ 0x01)}));
    const Bytes big(300, 0x11);
    const Bytes f = encode_frame(9, sp(big));
    ZTH_EXPECT_EQ(f.size(), std::size_t{1 + 1 + 2 + 300 + 1});
    ZTH_EXPECT_EQ(static_cast<int>(f[2]), 0xAC);   // 300 = AC 02
    ZTH_EXPECT_EQ(static_cast<int>(f[3]), 0x02);
}
ZTH_TEST("works", "a frame decodes back, even when it is fed byte by byte") {
    const Bytes payload = bytes_of({'h', 'e', 'l', 'l', 'o'});
    const Bytes wire = encode_frame(42, sp(payload));
    FrameDecoder d;
    for (std::size_t i = 0; i < wire.size(); ++i) {
        ZTH_EXPECT_EQ(d.feed(std::span<const std::uint8_t>(&wire[i], 1)), std::size_t{1});
        ZTH_EXPECT_EQ(d.next().has_value(), i + 1 == wire.size());
        if (i + 1 < wire.size()) ZTH_EXPECT_TRUE(d.error() == FrameError::None);
    }
    FrameDecoder e;
    e.feed(sp(wire));
    const auto f = e.next();
    ZTH_EXPECT_TRUE(f.has_value());
    if (!f) return;
    ZTH_EXPECT_EQ(static_cast<int>(f->type), 42);
    ZTH_EXPECT_TRUE(f->payload == payload);
    ZTH_EXPECT_FALSE(e.next().has_value());
}
ZTH_TEST("works", "several frames in one stream come out in order") {
    Bytes stream;
    for (int i = 0; i < 5; ++i) {
        const Bytes p(static_cast<std::size_t>(i * 3), static_cast<std::uint8_t>(i));
        const Bytes f = encode_frame(static_cast<std::uint8_t>(10 + i), sp(p));
        stream.insert(stream.end(), f.begin(), f.end());
    }
    FrameDecoder d;
    ZTH_EXPECT_EQ(d.feed(sp(stream)), stream.size());
    for (int i = 0; i < 5; ++i) {
        const auto f = d.next();
        ZTH_EXPECT_TRUE(f.has_value());
        if (!f) return;
        ZTH_EXPECT_EQ(static_cast<int>(f->type), 10 + i);
        ZTH_EXPECT_EQ(f->payload.size(), static_cast<std::size_t>(i * 3));
    }
    ZTH_EXPECT_FALSE(d.next().has_value());
}
ZTH_TEST("works", "an empty payload is a valid frame") {
    FrameDecoder d;
    d.feed(sp(encode_frame(3, sp(Bytes{}))));
    const auto f = d.next();
    ZTH_EXPECT_TRUE(f.has_value() && f->payload.empty() && f->type == 3);
}
ZTH_TEST("works", "frame errors: wrong version, wrong checksum, bad lengths, a length that is too large") {
    Bytes ok = encode_frame(1, sp(bytes_of({1, 2, 3})));
    {
        Bytes b = ok;
        b[0] = 2;
        FrameDecoder d;
        const std::size_t used = d.feed(sp(b));
        ZTH_EXPECT_TRUE(d.error() == FrameError::BadVersion);
        ZTH_EXPECT_EQ(used, std::size_t{1});   // it stops at the bad byte
        ZTH_EXPECT_EQ(d.feed(sp(ok)), std::size_t{0});   // and consumes nothing afterwards
        ZTH_EXPECT_FALSE(d.next().has_value());
    }
    {
        Bytes b = ok;
        b.back() ^= 1;
        FrameDecoder d;
        d.feed(sp(b));
        ZTH_EXPECT_TRUE(d.error() == FrameError::BadChecksum);
        ZTH_EXPECT_FALSE(d.next().has_value());
    }
    {
        Bytes b = ok;
        b[4] ^= 1;   // a payload byte: the checksum no longer matches
        FrameDecoder d;
        d.feed(sp(b));
        ZTH_EXPECT_TRUE(d.error() == FrameError::BadChecksum);
    }
    {
        FrameDecoder d;
        d.feed(sp(bytes_of({1, 5, 0x80, 0x00})));   // length 0 written in two bytes
        ZTH_EXPECT_TRUE(d.error() == FrameError::BadLength);
    }
    {
        FrameDecoder d;
        d.feed(sp(bytes_of({1, 5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02})));
        ZTH_EXPECT_TRUE(d.error() == FrameError::BadLength);
    }
    {
        FrameDecoder d(100);
        d.feed(sp(bytes_of({1, 5, 101})));
        ZTH_EXPECT_TRUE(d.error() == FrameError::TooLarge);
        FrameDecoder e(100);
        e.feed(sp(encode_frame(5, sp(Bytes(100, 1)))));
        ZTH_EXPECT_TRUE(e.error() == FrameError::None && e.next().has_value());   // exactly the limit is fine
    }
}
ZTH_TEST("works", "reset() clears an error and any half-built frame") {
    FrameDecoder d;
    d.feed(sp(bytes_of({9})));
    ZTH_EXPECT_TRUE(d.error() == FrameError::BadVersion);
    d.reset();
    ZTH_EXPECT_TRUE(d.error() == FrameError::None);
    d.feed(sp(encode_frame(1, sp(Bytes{7}))));
    ZTH_EXPECT_TRUE(d.next().has_value());
    FrameDecoder g;
    g.feed(sp(encode_frame(1, sp(Bytes{7}))));   // a finished frame nobody has collected yet
    g.reset();
    ZTH_EXPECT_FALSE(g.next().has_value());
    FrameDecoder e;
    e.feed(sp(bytes_of({1, 2, 5, 'a'})));   // half a frame
    ZTH_EXPECT_EQ(e.buffered_bytes(), std::size_t{1});
    e.reset();
    ZTH_EXPECT_EQ(e.buffered_bytes(), std::size_t{0});
    e.feed(sp(encode_frame(1, sp(Bytes{7}))));
    ZTH_EXPECT_TRUE(e.next().has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "20000 random values of every kind round-trip") {
    zth::Lcg rng(2025);
    for (int i = 0; i < 20000; ++i) {
        const std::uint64_t u = (static_cast<std::uint64_t>(rng.next()) << 33) ^ (static_cast<std::uint64_t>(rng.next()) << 11) ^ static_cast<std::uint64_t>(rng.next()) >> rng.below(30);
        const std::uint64_t shifted = u >> rng.below(64);
        const std::int64_t s = static_cast<std::int64_t>(shifted) * (rng.below(2) ? 1 : -1);
        ByteWriter w;
        w.put_varint(shifted);
        w.put_svarint(s);
        w.put_u16be(static_cast<std::uint16_t>(u));
        w.put_u32le(static_cast<std::uint32_t>(u >> 3));
        w.put_u64be(u);
        w.put_u64le(shifted);
        ByteReader r(sp(w.bytes()));
        const auto a = r.get_varint();
        const auto b = r.get_svarint();
        const auto c = r.get_u16be();
        const auto d = r.get_u32le();
        const auto e = r.get_u64be();
        const auto f = r.get_u64le();
        if (!a || !b || !c || !d || !e || !f || *a != shifted || *b != s || *c != static_cast<std::uint16_t>(u) || *d != static_cast<std::uint32_t>(u >> 3) || *e != u || *f != shifted || !r.at_end()) {
            zth::fail("a value did not round-trip in round " + std::to_string(i));
            return;
        }
    }
}
ZTH_TEST("robust", "every varint has exactly the minimal length: ceil(bits / 7) bytes") {
    for (int bits = 0; bits <= 64; ++bits) {
        const std::uint64_t v = bits == 0 ? 0 : (bits == 64 ? std::numeric_limits<std::uint64_t>::max() : (1ULL << bits) - 1);
        const std::size_t want = bits == 0 ? 1 : static_cast<std::size_t>((bits + 6) / 7);
        if (varint(v).size() != want) {
            zth::fail("a value of " + std::to_string(bits) + " bits takes " + std::to_string(varint(v).size()) + " bytes, expected " + std::to_string(want));
            return;
        }
    }
}
ZTH_TEST("robust", "arbitrary garbage never makes the reader move on a failure, read out of bounds, or loop") {
    zth::Lcg rng(9);
    for (int i = 0; i < 5000; ++i) {
        Bytes b(static_cast<std::size_t>(rng.below(16)));
        for (auto& x : b) x = static_cast<std::uint8_t>(rng.below(4) == 0 ? 0x80 | rng.below(128) : rng.below(256));
        ByteReader r(sp(b));
        for (int step = 0; step < 6; ++step) {
            const std::size_t before = r.position();
            bool ok = false;
            switch (rng.below(6)) {
                case 0: ok = r.get_varint().has_value(); break;
                case 1: ok = r.get_svarint().has_value(); break;
                case 2: ok = r.get_string().has_value(); break;
                case 3: ok = r.get_u32be().has_value(); break;
                case 4: ok = r.get_u64le().has_value(); break;
                default: ok = r.get_bytes(static_cast<std::size_t>(rng.below(20))).has_value(); break;
            }
            if (!ok && r.position() != before) {
                zth::fail("a failed read moved the position");
                return;
            }
            if (r.position() > b.size()) {
                zth::fail("the position is beyond the end");
                return;
            }
        }
    }
}
ZTH_TEST("robust", "any way of cutting a stream of frames gives the same frames") {
    zth::Lcg rng(17);
    Bytes stream;
    std::vector<Bytes> payloads;
    for (int i = 0; i < 40; ++i) {
        Bytes p(static_cast<std::size_t>(rng.below(300)));
        for (auto& x : p) x = static_cast<std::uint8_t>(rng.below(256));
        const Bytes f = encode_frame(static_cast<std::uint8_t>(i), sp(p));
        stream.insert(stream.end(), f.begin(), f.end());
        payloads.push_back(p);
    }
    for (int round = 0; round < 40; ++round) {
        FrameDecoder d;
        std::size_t pos = 0;
        int got = 0;
        while (pos < stream.size()) {
            const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(1 + rng.below(round % 2 ? 7 : 400)), stream.size() - pos);
            if (d.feed(std::span<const std::uint8_t>(stream.data() + pos, n)) != n) {
                zth::fail("the decoder did not consume everything");
                return;
            }
            pos += n;
            while (auto f = d.next()) {
                if (f->type != got || f->payload != payloads[static_cast<std::size_t>(got)]) {
                    zth::fail("frame " + std::to_string(got) + " is wrong in round " + std::to_string(round));
                    return;
                }
                ++got;
            }
        }
        ZTH_EXPECT_EQ(got, 40);
    }
}
ZTH_TEST("robust", "flipping any single bit of a frame is always detected or changes the content (never a silent wrong frame)") {
    const Bytes payload = bytes_of({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    const Bytes good = encode_frame(5, sp(payload));
    for (std::size_t bit = 0; bit < good.size() * 8; ++bit) {
        Bytes bad = good;
        bad[bit / 8] = static_cast<std::uint8_t>(bad[bit / 8] ^ (1u << (bit % 8)));
        FrameDecoder d;
        d.feed(sp(bad));
        const auto f = d.next();
        if (f && f->type == 5 && f->payload == payload) {
            zth::fail("flipping bit " + std::to_string(bit) + " went unnoticed");
            return;
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "reading allocates nothing: integers, varints, strings and byte ranges are views") {
    ByteWriter w;
    for (int i = 0; i < 1000; ++i) {
        w.put_varint(static_cast<std::uint64_t>(i) * 1000003);
        w.put_u32be(static_cast<std::uint32_t>(i));
        w.put_string("a string long enough to need the heap if anybody copied it, index");
    }
    zth::mem::Scope scope;
    ByteReader r(sp(w.bytes()));
    std::uint64_t sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum += val(r.get_varint());
        sum += val(r.get_u32be());
        sum += r.get_string()->size();
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sum > 0 && r.at_end());
}
ZTH_TEST("fast", "writing with reserve() allocates once") {
    ByteWriter w;
    w.reserve(100000);
    zth::mem::Scope scope;
    for (int i = 0; i < 20000; ++i) {
        w.put_varint(static_cast<std::uint64_t>(i));
        w.put_u16be(static_cast<std::uint16_t>(i));
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(w.size() > 60000 && w.size() <= 100000);
}
ZTH_TEST("fast", "a frame that announces a huge payload is rejected before anything is stored for it") {
    FrameDecoder d(1000);
    const Bytes hostile = bytes_of({1, 5, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F});   // length 4 GB - 1
    zth::mem::Scope scope;
    d.feed(sp(hostile));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(d.error() == FrameError::TooLarge);
    ZTH_EXPECT_EQ(d.buffered_bytes(), std::size_t{0});
}
ZTH_TEST_TIMEOUT("fast", "decoding 20 MB of frames in 1 KB pieces is linear time", 20) {
    Bytes payload(1000, 0x42);
    const Bytes f = encode_frame(1, sp(payload));
    Bytes stream;
    for (int i = 0; i < 20000; ++i) stream.insert(stream.end(), f.begin(), f.end());
    FrameDecoder d;
    std::size_t frames = 0;
    for (std::size_t pos = 0; pos < stream.size(); pos += 1024) {
        d.feed(std::span<const std::uint8_t>(stream.data() + pos, std::min<std::size_t>(1024, stream.size() - pos)));
        while (d.next()) ++frames;
    }
    ZTH_EXPECT_EQ(frames, std::size_t{20000});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("codec.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("codec.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "byte order by shifts, not by copying the host representation: no memcpy, no unions, no pointer casts of integers, no htons-style calls") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("codec.hpp"));
    for (const char* word : {"memcpy", "memmove", "bit_cast", "union", "htons", "htonl", "ntohs", "ntohl", "byteswap", "endian", "__builtin_bswap32", "__builtin_bswap64"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "views not copies: the reader holds a span") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("codec.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "span").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
}
