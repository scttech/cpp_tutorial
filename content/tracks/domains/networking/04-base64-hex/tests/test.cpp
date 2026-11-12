#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <string>
#include <vector>

#include "codec64.hpp"
#include "zth_test.hpp"

using Bytes = std::vector<std::uint8_t>;
using b64::Alphabet;
using b64::DecodeOptions;

namespace {

Bytes bytes_of(const std::string& s) { return Bytes(s.begin(), s.end()); }
std::span<const std::uint8_t> sp(const Bytes& b) { return std::span<const std::uint8_t>(b.data(), b.size()); }
std::string enc(const std::string& s, Alphabet a = Alphabet::Standard, bool pad = true) { return b64::encode(sp(bytes_of(s)), a, pad); }
std::string dec(std::string_view t, Alphabet a = Alphabet::Standard, DecodeOptions o = {}) {
    const auto r = b64::decode(t, a, o);
    return r ? std::string(r->begin(), r->end()) : std::string("<invalid>");
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "the RFC 4648 test vectors, padded") {
    ZTH_EXPECT_EQ(enc(""), std::string(""));
    ZTH_EXPECT_EQ(enc("f"), std::string("Zg=="));
    ZTH_EXPECT_EQ(enc("fo"), std::string("Zm8="));
    ZTH_EXPECT_EQ(enc("foo"), std::string("Zm9v"));
    ZTH_EXPECT_EQ(enc("foob"), std::string("Zm9vYg=="));
    ZTH_EXPECT_EQ(enc("fooba"), std::string("Zm9vYmE="));
    ZTH_EXPECT_EQ(enc("foobar"), std::string("Zm9vYmFy"));
}
ZTH_TEST("works", "unpadded output leaves the '=' out and nothing else") {
    ZTH_EXPECT_EQ(enc("f", Alphabet::Standard, false), std::string("Zg"));
    ZTH_EXPECT_EQ(enc("fo", Alphabet::Standard, false), std::string("Zm8"));
    ZTH_EXPECT_EQ(enc("foo", Alphabet::Standard, false), std::string("Zm9v"));
    ZTH_EXPECT_EQ(enc("foob", Alphabet::Standard, false), std::string("Zm9vYg"));
    ZTH_EXPECT_EQ(enc("", Alphabet::Standard, false), std::string(""));
}
ZTH_TEST("works", "all 64 values appear in the right place in each alphabet") {
    Bytes all;
    for (int i = 0; i < 48; ++i) all.push_back(static_cast<std::uint8_t>(i * 5 + 3));
    const std::string std_all = b64::encode(sp(all), Alphabet::Standard);
    ZTH_EXPECT_EQ(std_all.size(), std::size_t{64});
    // a message whose sextets are 0..63 in order
    const Bytes seq = {0x00, 0x10, 0x83, 0x10, 0x51, 0x87, 0x20, 0x92, 0x8B, 0x30, 0xD3, 0x8F, 0x41, 0x14, 0x93, 0x51, 0x55, 0x97, 0x61, 0x96, 0x9B, 0x71, 0xD7, 0x9F, 0x82, 0x18, 0xA3, 0x92, 0x59, 0xA7, 0xA2, 0x9A, 0xAB, 0xB2, 0xDB, 0xAF, 0xC3, 0x1C, 0xB3, 0xD3, 0x5D, 0xB7, 0xE3, 0x9E, 0xBB, 0xF3, 0xDF, 0xBF};
    ZTH_EXPECT_EQ(b64::encode(sp(seq)), std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"));
    ZTH_EXPECT_EQ(b64::encode(sp(seq), Alphabet::UrlSafe), std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"));
}
ZTH_TEST("works", "the URL-safe alphabet differs in exactly '+' -> '-' and '/' -> '_'") {
    const Bytes b = {0xFB, 0xFF, 0xBF};
    ZTH_EXPECT_EQ(b64::encode(sp(b)), std::string("+/+/"));
    ZTH_EXPECT_EQ(b64::encode(sp(b), Alphabet::UrlSafe), std::string("-_-_"));
    const Bytes c = {0xFF, 0xEE};
    ZTH_EXPECT_EQ(b64::encode(sp(c)), std::string("/+4="));
    ZTH_EXPECT_EQ(b64::encode(sp(c), Alphabet::UrlSafe), std::string("_-4="));
    ZTH_EXPECT_EQ(b64::encode(sp(c), Alphabet::UrlSafe, false), std::string("_-4"));
}
ZTH_TEST("works", "encoded_size and max_decoded_size") {
    ZTH_EXPECT_EQ(b64::encoded_size(0), std::size_t{0});
    ZTH_EXPECT_EQ(b64::encoded_size(1), std::size_t{4});
    ZTH_EXPECT_EQ(b64::encoded_size(2), std::size_t{4});
    ZTH_EXPECT_EQ(b64::encoded_size(3), std::size_t{4});
    ZTH_EXPECT_EQ(b64::encoded_size(4), std::size_t{8});
    ZTH_EXPECT_EQ(b64::encoded_size(1, false), std::size_t{2});
    ZTH_EXPECT_EQ(b64::encoded_size(2, false), std::size_t{3});
    ZTH_EXPECT_EQ(b64::encoded_size(3, false), std::size_t{4});
    ZTH_EXPECT_EQ(b64::encoded_size(4, false), std::size_t{6});
    ZTH_EXPECT_EQ(b64::encoded_size(1000, true), std::size_t{1336});
    ZTH_EXPECT_EQ(b64::encoded_size(1000, false), std::size_t{1334});
    ZTH_EXPECT_EQ(b64::max_decoded_size(0), std::size_t{0});
    ZTH_EXPECT_EQ(b64::max_decoded_size(2), std::size_t{1});
    ZTH_EXPECT_EQ(b64::max_decoded_size(3), std::size_t{2});
    ZTH_EXPECT_EQ(b64::max_decoded_size(4), std::size_t{3});
    ZTH_EXPECT_EQ(b64::max_decoded_size(8), std::size_t{6});
    ZTH_EXPECT_EQ(b64::max_decoded_size(1336), std::size_t{1002});
}
ZTH_TEST("works", "decoding the RFC vectors, padded and unpadded") {
    ZTH_EXPECT_EQ(dec(""), std::string(""));
    ZTH_EXPECT_EQ(dec("Zg=="), std::string("f"));
    ZTH_EXPECT_EQ(dec("Zm8="), std::string("fo"));
    ZTH_EXPECT_EQ(dec("Zm9v"), std::string("foo"));
    ZTH_EXPECT_EQ(dec("Zm9vYg=="), std::string("foob"));
    ZTH_EXPECT_EQ(dec("Zm9vYmE="), std::string("fooba"));
    ZTH_EXPECT_EQ(dec("Zm9vYmFy"), std::string("foobar"));
    DecodeOptions loose;
    loose.require_padding = false;
    ZTH_EXPECT_EQ(dec("Zg", Alphabet::Standard, loose), std::string("f"));
    ZTH_EXPECT_EQ(dec("Zm8", Alphabet::Standard, loose), std::string("fo"));
    ZTH_EXPECT_EQ(dec("Zm9vYg", Alphabet::Standard, loose), std::string("foob"));
    ZTH_EXPECT_EQ(dec("Zg==", Alphabet::Standard, loose), std::string("f"));   // correct padding is still fine
    ZTH_EXPECT_EQ(dec("Zm9v", Alphabet::Standard, loose), std::string("foo"));
}
ZTH_TEST("works", "padding errors") {
    for (const char* bad : {"Zg=", "Zg===", "Z===", "Zm9v=", "Zm9v==", "Zm9vYg=", "Zm8==", "=", "==", "====", "Zg=g", "=Zg=", "Zm=v", "Zg==Zg==", "Zg== ", "Zg==\n"}) {
        if (dec(bad) != "<invalid>") {
            zth::fail(std::string("`") + bad + "` should be rejected, got `" + dec(bad) + "`");
            return;
        }
    }
    DecodeOptions loose;
    loose.require_padding = false;
    for (const char* bad : {"Zg=", "Zm8==", "Z", "Zm9vY", "Zg=g", "Zg===", "=Zg", "Zm=v"}) {
        if (dec(bad, Alphabet::Standard, loose) != "<invalid>") {
            zth::fail(std::string("`") + bad + "` should be rejected even without required padding");
            return;
        }
    }
}
ZTH_TEST("works", "a group of one character can never be valid") {
    for (const char* bad : {"Z", "Zm9vY", "Zm9vYmFyZ", "Z===", "Zm9vZ===", "A", "AAAAA", "A===", "AAAAAAAAA"}) ZTH_EXPECT_EQ(dec(bad), std::string("<invalid>"));   // the zero-valued A passes the trailing-bits test, only the length rule catches it
    DecodeOptions loose;
    loose.require_padding = false;
    ZTH_EXPECT_EQ(dec("Z", Alphabet::Standard, loose), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec("A", Alphabet::Standard, loose), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec("AAAAA", Alphabet::Standard, loose), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec("Zm9vY", Alphabet::Standard, loose), std::string("<invalid>"));
}
ZTH_TEST("works", "canonical form: the unused trailing bits must be zero") {
    for (const char* bad : {"Zh==", "Zi==", "Zj==", "Zm9=", "Zm+=", "Zm/=", "Zm9vYh==", "Zm9vYmF="}) {
        if (dec(bad) != "<invalid>") {
            zth::fail(std::string("`") + bad + "` is not the canonical encoding of anything");
            return;
        }
    }
    DecodeOptions loose;
    loose.require_padding = false;
    ZTH_EXPECT_EQ(dec("Zh", Alphabet::Standard, loose), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec("Zm9", Alphabet::Standard, loose), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec("Zw==", Alphabet::Standard), std::string("g"));   // Z w: the last four bits are zero, so this is the canonical encoding of "g"
    ZTH_EXPECT_EQ(dec("Zg=="), std::string("f"));   // 'g' = 100000: the low four bits are zero
    ZTH_EXPECT_EQ(dec("Zm8="), std::string("fo"));
    ZTH_EXPECT_EQ(dec("/w=="), std::string("\xFF"));
    ZTH_EXPECT_EQ(dec("//8="), std::string("\xFF\xFF"));
}
ZTH_TEST("works", "characters outside the chosen alphabet are rejected, including the other alphabet's and anything non-ASCII") {
    ZTH_EXPECT_EQ(dec("+/+/"), std::string("\xFB\xFF\xBF"));
    ZTH_EXPECT_EQ(dec("-_-_"), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec("-_-_", Alphabet::UrlSafe), std::string("\xFB\xFF\xBF"));
    ZTH_EXPECT_EQ(dec("+/+/", Alphabet::UrlSafe), std::string("<invalid>"));
    for (const char* bad : {"Zm9!", "Zm9v!", "Zm 9v", "Zm9v\n", "Zm9\t", "Zm9v.", "Zm9*", "Zm9\xC3\xA9", "Zm9\x80", "Zm9\xFF", "Zm9@", "Zm9\xC1", "\xC1\xC1\xC1\xC1", "\xE1\xE2\xE3\xE4"}) {
        if (dec(bad) != "<invalid>") {
            zth::fail(std::string("`") + bad + "` should be rejected");
            return;
        }
    }
    ZTH_EXPECT_EQ(dec(std::string_view("Zm9\0", 4)), std::string("<invalid>"));
    ZTH_EXPECT_EQ(dec(std::string_view("Zm9v\0", 5)), std::string("<invalid>"));
}
ZTH_TEST("works", "whitespace is rejected unless allowed, and then it may appear anywhere") {
    DecodeOptions ws;
    ws.allow_whitespace = true;
    ZTH_EXPECT_EQ(dec("Zm9v\r\nYmFy\r\n", Alphabet::Standard, ws), std::string("foobar"));
    ZTH_EXPECT_EQ(dec(" Z m 9 v ", Alphabet::Standard, ws), std::string("foo"));
    ZTH_EXPECT_EQ(dec("Zg =\t=", Alphabet::Standard, ws), std::string("f"));   // also between padding characters
    ZTH_EXPECT_EQ(dec("Zg==\n", Alphabet::Standard, ws), std::string("f"));
    ZTH_EXPECT_EQ(dec("\n\n", Alphabet::Standard, ws), std::string(""));
    ZTH_EXPECT_EQ(dec("Zg= =g", Alphabet::Standard, ws), std::string("<invalid>"));   // whitespace does not excuse data after padding
    ZTH_EXPECT_EQ(dec("Zm9\x0B" "v", Alphabet::Standard, ws), std::string("<invalid>"));   // only space, tab, CR and LF count as whitespace
    ZTH_EXPECT_EQ(dec("Zm9v YmFy"), std::string("<invalid>"));
}
ZTH_TEST("works", "decode_to and encode_to write into caller buffers and report how much") {
    char text[16];
    const Bytes b = bytes_of("foob");
    const auto n = b64::encode_to(sp(b), std::span<char>(text, sizeof text));
    ZTH_EXPECT_TRUE(n.has_value() && *n == 8);
    ZTH_EXPECT_EQ(std::string(text, 8), std::string("Zm9vYg=="));
    std::uint8_t raw[8];
    const auto m = b64::decode_to("Zm9vYg==", std::span<std::uint8_t>(raw, sizeof raw));
    ZTH_EXPECT_TRUE(m.has_value() && *m == 4);
    ZTH_EXPECT_EQ(std::string(reinterpret_cast<const char*>(raw), 4), std::string("foob"));
    ZTH_EXPECT_FALSE(b64::decode_to("Zm9v!", std::span<std::uint8_t>(raw, sizeof raw)).has_value());
}
ZTH_TEST("works", "buffers that are too small are reported, never overrun") {
    const Bytes b = bytes_of("foobar");
    char small[7];
    ZTH_EXPECT_FALSE(b64::encode_to(sp(b), std::span<char>(small, 7)).has_value());   // needs 8
    char exact[8];
    ZTH_EXPECT_TRUE(b64::encode_to(sp(b), std::span<char>(exact, 8)).has_value());
    char empty_target[1];
    ZTH_EXPECT_TRUE(b64::encode_to(sp(Bytes{}), std::span<char>(empty_target, 0)).has_value());
    std::uint8_t out5[5];
    ZTH_EXPECT_FALSE(b64::decode_to("Zm9vYmFy", std::span<std::uint8_t>(out5, 5)).has_value());   // 6 bytes do not fit in 5
    std::uint8_t out6[6];
    const auto n = b64::decode_to("Zm9vYmFy", std::span<std::uint8_t>(out6, 6));
    ZTH_EXPECT_TRUE(n.has_value() && *n == 6);
    ZTH_EXPECT_FALSE(b64::decode_to("Zg==", std::span<std::uint8_t>(out6, 0)).has_value());
    ZTH_EXPECT_TRUE(b64::decode_to("", std::span<std::uint8_t>(out6, 0)).has_value());
}
ZTH_TEST("works", "hex encoding, lower and upper case") {
    ZTH_EXPECT_EQ(hex::encode(sp(bytes_of("foobar"))), std::string("666f6f626172"));
    ZTH_EXPECT_EQ(hex::encode(sp(bytes_of("foobar")), true), std::string("666F6F626172"));
    ZTH_EXPECT_EQ(hex::encode(sp(Bytes{})), std::string(""));
    ZTH_EXPECT_EQ(hex::encode(sp(Bytes{0x00, 0x0F, 0xF0, 0xFF, 0x80, 0x7F})), std::string("000ff0ff807f"));
    ZTH_EXPECT_EQ(hex::encode(sp(Bytes{0xAB, 0xCD, 0xEF}), true), std::string("ABCDEF"));
}
ZTH_TEST("works", "hex decoding accepts either case and nothing else") {
    const auto ok = hex::decode("666F6f626172");
    ZTH_EXPECT_TRUE(ok.has_value() && *ok == bytes_of("foobar"));
    ZTH_EXPECT_TRUE(hex::decode("").has_value());
    const auto all = hex::decode("00ff7F80aA");
    ZTH_EXPECT_TRUE(all.has_value() && *all == (Bytes{0x00, 0xFF, 0x7F, 0x80, 0xAA}));
    for (const char* bad : {"6", "666", "6g", "g6", "0x66", "66 66", "66\n", " 66", "6-", "zz", "66G6", "\xC3\xA9"}) {
        if (hex::decode(bad).has_value()) {
            zth::fail(std::string("`") + bad + "` should be rejected");
            return;
        }
    }
    ZTH_EXPECT_FALSE(hex::decode(std::string_view("66\0", 3)).has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "decode(encode(x)) == x for all lengths up to 300 and random bytes, in both alphabets, padded and unpadded") {
    zth::Lcg rng(123);
    for (std::size_t n = 0; n <= 300; ++n) {
        Bytes b(n);
        for (auto& x : b) x = static_cast<std::uint8_t>(rng.below(256));
        for (Alphabet a : {Alphabet::Standard, Alphabet::UrlSafe}) {
            const std::string padded = b64::encode(sp(b), a, true);
            const std::string bare = b64::encode(sp(b), a, false);
            if (padded.size() != b64::encoded_size(n, true) || bare.size() != b64::encoded_size(n, false)) {
                zth::fail("the encoded size is wrong for " + std::to_string(n) + " bytes");
                return;
            }
            DecodeOptions loose;
            loose.require_padding = false;
            const auto p = b64::decode(padded, a);
            const auto q = b64::decode(bare, a, loose);
            if (!p || !q || *p != b || *q != b || b64::decode(bare, a).has_value() != (padded == bare)) {
                zth::fail("round trip failed for " + std::to_string(n) + " bytes");
                return;
            }
            hex::decode(hex::encode(sp(b)));
            const auto h = hex::decode(hex::encode(sp(b), n % 2 == 0));
            if (!h || *h != b) {
                zth::fail("hex round trip failed for " + std::to_string(n) + " bytes");
                return;
            }
        }
    }
}
ZTH_TEST("robust", "the encoder uses only alphabet characters and padding, and its output is canonical (decoding then encoding gives the same text)") {
    zth::Lcg rng(7);
    for (int round = 0; round < 3000; ++round) {
        Bytes b(static_cast<std::size_t>(rng.below(50)));
        for (auto& x : b) x = static_cast<std::uint8_t>(rng.below(256));
        const std::string t = b64::encode(sp(b));
        for (char c : t) {
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
            if (!ok) {
                zth::fail("the encoder produced the character " + std::to_string(static_cast<int>(c)));
                return;
            }
        }
        const auto back = b64::decode(t);
        if (!back || b64::encode(sp(*back)) != t) {
            zth::fail("the encoding is not canonical");
            return;
        }
    }
}
ZTH_TEST("robust", "exactly one text decodes to each byte string: flipping any single character of a valid encoding either fails or changes the bytes") {
    zth::Lcg rng(31);
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int round = 0; round < 300; ++round) {
        Bytes b(static_cast<std::size_t>(1 + rng.below(20)));
        for (auto& x : b) x = static_cast<std::uint8_t>(rng.below(256));
        const std::string t = b64::encode(sp(b));
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (t[i] == '=') continue;
            for (char c : alphabet) {
                if (c == t[i]) continue;
                std::string u = t;
                u[i] = c;
                const auto r = b64::decode(u);
                if (r && *r == b) {
                    zth::fail("`" + u + "` decodes to the same bytes as `" + t + "`: two texts for one message");
                    return;
                }
            }
        }
    }
}
ZTH_TEST("robust", "a guard zone around the output buffers is never touched: encode_to, decode_to for every size from 0 to 80") {
    zth::Lcg rng(2);
    for (std::size_t n = 0; n <= 80; ++n) {
        Bytes b(n);
        for (auto& x : b) x = static_cast<std::uint8_t>(rng.below(256));
        const std::size_t need = b64::encoded_size(n, true);
        for (std::size_t cap : {need, need + 1, need > 0 ? need - 1 : 0}) {
            std::vector<char> buf(cap + 16, '#');
            const auto r = b64::encode_to(sp(b), std::span<char>(buf.data() + 8, cap));
            for (std::size_t i = 0; i < 8; ++i) ZTH_EXPECT_EQ(buf[i], '#');
            for (std::size_t i = 8 + cap; i < buf.size(); ++i) ZTH_EXPECT_EQ(buf[i], '#');
            ZTH_EXPECT_EQ(r.has_value(), cap >= need);
            if (r) {
                ZTH_EXPECT_EQ(*r, need);
                for (std::size_t i = 8 + need; i < 8 + cap; ++i) ZTH_EXPECT_EQ(buf[i], '#');   // not even one character more than needed
            }
        }
        const std::size_t need_bare = b64::encoded_size(n, false);
        for (std::size_t cap : {need_bare, need_bare + 1, need_bare > 0 ? need_bare - 1 : 0}) {
            std::vector<char> buf(cap + 16, '#');
            const auto r = b64::encode_to(sp(b), std::span<char>(buf.data() + 8, cap), Alphabet::Standard, false);
            for (std::size_t i = 0; i < 8; ++i) ZTH_EXPECT_EQ(buf[i], '#');
            for (std::size_t i = 8 + cap; i < buf.size(); ++i) ZTH_EXPECT_EQ(buf[i], '#');
            ZTH_EXPECT_EQ(r.has_value(), cap >= need_bare);
            if (r) {
                ZTH_EXPECT_EQ(*r, need_bare);
                for (std::size_t i = 8 + need_bare; i < 8 + cap; ++i) ZTH_EXPECT_EQ(buf[i], '#');
            }
        }
        const std::string text = b64::encode(sp(b));
        for (std::size_t cap : {n, n + 1, n > 0 ? n - 1 : 0}) {
            std::vector<std::uint8_t> out(cap + 16, 0xEE);
            const auto r = b64::decode_to(text, std::span<std::uint8_t>(out.data() + 8, cap));
            for (std::size_t i = 0; i < 8; ++i) ZTH_EXPECT_EQ(static_cast<int>(out[i]), 0xEE);
            for (std::size_t i = 8 + cap; i < out.size(); ++i) ZTH_EXPECT_EQ(static_cast<int>(out[i]), 0xEE);
            ZTH_EXPECT_EQ(r.has_value(), cap >= n);
        }
    }
}
ZTH_TEST("robust", "garbage input never crashes the decoders: 20000 random strings") {
    zth::Lcg rng(77);
    const std::string alphabet = "ABCDabcd0189+/-_=== \n\t\x01\xFF";
    for (int round = 0; round < 20000; ++round) {
        std::string s(static_cast<std::size_t>(rng.below(24)), 'A');
        for (char& c : s) c = alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))];
        DecodeOptions o;
        o.allow_whitespace = rng.below(2) == 0;
        o.require_padding = rng.below(2) == 0;
        const auto r = b64::decode(s, rng.below(2) ? Alphabet::Standard : Alphabet::UrlSafe, o);
        if (r && r->size() > b64::max_decoded_size(s.size())) {
            zth::fail("decoded more than max_decoded_size says");
            return;
        }
        (void)hex::decode(s);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "encode_to, decode_to and the size functions allocate nothing") {
    Bytes data(3000);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i * 7);
    std::vector<char> text(b64::encoded_size(data.size()));
    std::vector<std::uint8_t> back(data.size());
    zth::mem::Scope scope;
    std::size_t total = 0;
    for (int round = 0; round < 100; ++round) {
        total += *b64::encode_to(sp(data), std::span<char>(text.data(), text.size()));
        total += *b64::decode_to(std::string_view(text.data(), text.size()), std::span<std::uint8_t>(back.data(), back.size()));
        total += b64::encoded_size(static_cast<std::size_t>(round)) + b64::max_decoded_size(static_cast<std::size_t>(round));
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(total > 0 && back == data);
}
ZTH_TEST("fast", "encode and decode allocate their result once (plus the temporary of decode)") {
    Bytes data(3000, 0x5A);
    zth::mem::Scope scope;
    const std::string t = b64::encode(sp(data));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    zth::mem::Scope scope2;
    const auto r = b64::decode(t);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope2, 1);
    ZTH_EXPECT_TRUE(r.has_value() && *r == data);
}
ZTH_TEST_TIMEOUT("fast", "48 MB of data through both codecs: a table lookup per character, not a search", 20) {
    Bytes data(48 * 1024 * 1024);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i * 2654435761u >> 24);
    const std::string t = b64::encode(sp(data));
    ZTH_EXPECT_EQ(t.size(), b64::encoded_size(data.size()));
    const auto back = b64::decode(t);
    ZTH_EXPECT_TRUE(back.has_value() && back->size() == data.size() && *back == data);
    const std::string h = hex::encode(std::span<const std::uint8_t>(data.data(), 8 * 1024 * 1024));
    const auto hb = hex::decode(h);
    ZTH_EXPECT_TRUE(hb.has_value() && hb->size() == 8 * 1024 * 1024);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("codec64.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("codec64.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "decoding uses a table built at compile time: no searching the alphabet (find, strchr, memchr, index_of), no <algorithm>-style scans") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("codec64.hpp"));
    for (const char* word : {"find", "strchr", "memchr", "find_first_of", "binary_search", "lower_bound", "find_if", "count"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "constexpr").empty());
}
ZTH_TEST("clean", "no library codecs or streams: no sstream, no iostream, no stoi, no format helpers") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("codec64.hpp"));
    for (const char* word : {"stringstream", "iostream", "sstream", "stoi", "stoul", "sprintf", "snprintf", "to_string", "format", "regex"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
