#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <string>
#include <vector>

#include "crc.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace crc;

namespace {

// An independent, bit-by-bit implementation of the Rocksoft model. It does not use the engine under test.
template <class T>
T ref_reflect(T v, unsigned bits) {
    T r = 0;
    for (unsigned i = 0; i < bits; ++i) {
        r = static_cast<T>((r << 1) | (v & 1));
        v = static_cast<T>(v >> 1);
    }
    return r;
}
template <class T>
T ref_crc(const CrcParams<T>& p, const std::vector<std::uint8_t>& data) {
    const unsigned bits = std::numeric_limits<T>::digits;
    const T top = static_cast<T>(T{1} << (bits - 1));
    T r = p.init;
    for (std::uint8_t b : data) {
        const std::uint8_t in = p.reflect_in ? ref_reflect<std::uint8_t>(b, 8) : b;
        r = static_cast<T>(r ^ static_cast<T>(static_cast<T>(in) << (bits - 8)));
        for (int k = 0; k < 8; ++k) r = (r & top) ? static_cast<T>((r << 1) ^ p.poly) : static_cast<T>(r << 1);
    }
    if (p.reflect_out) r = ref_reflect<T>(r, bits);
    return static_cast<T>(r ^ p.xor_out);
}

std::vector<std::uint8_t> bytes(const std::string& s) { return std::vector<std::uint8_t>(s.begin(), s.end()); }
const std::string kCheck = "123456789";

// parameter sets with unusual combinations (mixed reflection, asymmetric init) for the differential test
inline constexpr CrcParams<std::uint8_t> kM1{0x1D, 0x5A, true, false, 0x3C};
inline constexpr CrcParams<std::uint8_t> kM2{0x31, 0xC3, false, true, 0xFF};
inline constexpr CrcParams<std::uint16_t> kM3{0x3D65, 0x1234, true, false, 0x00FF};
inline constexpr CrcParams<std::uint16_t> kM4{0xA001, 0xABCD, false, true, 0x8001};
inline constexpr CrcParams<std::uint32_t> kM5{0x814141AB, 0x12345678, true, false, 0xDEADBEEF};
inline constexpr CrcParams<std::uint32_t> kM6{0xAF, 0x0F0F0F0F, false, true, 0x01};
inline constexpr CrcParams<std::uint32_t> kM7{0x04C11DB7, 0x00000001, true, true, 0x00000000};

template <class T, CrcParams<T> P>
bool agrees_with_reference(zth::Lcg& rng, std::string& why) {
    for (int round = 0; round < 60; ++round) {
        std::vector<std::uint8_t> data(static_cast<std::size_t>(rng.below(40)));
        for (auto& b : data) b = static_cast<std::uint8_t>(rng.below(256));
        const T want = ref_crc<T>(P, data);
        const T got = Crc<T, P>::compute(std::span<const std::uint8_t>(data.data(), data.size()));
        if (got != want) {
            why = "length " + std::to_string(data.size()) + ": got " + std::to_string(static_cast<unsigned long long>(got)) + ", expected " + std::to_string(static_cast<unsigned long long>(want));
            return false;
        }
    }
    return true;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "reflect reverses the bits of any width") {
    ZTH_EXPECT_EQ(reflect<std::uint8_t>(0x01), std::uint8_t{0x80});
    ZTH_EXPECT_EQ(reflect<std::uint8_t>(0xB4), std::uint8_t{0x2D});
    ZTH_EXPECT_EQ(reflect<std::uint16_t>(0x0001), std::uint16_t{0x8000});
    ZTH_EXPECT_EQ(reflect<std::uint16_t>(0x1021), std::uint16_t{0x8408});
    ZTH_EXPECT_EQ(reflect<std::uint32_t>(0x04C11DB7), std::uint32_t{0xEDB88320});
    ZTH_EXPECT_EQ(reflect<std::uint32_t>(0), std::uint32_t{0});
    ZTH_EXPECT_EQ(reflect<std::uint32_t>(0xFFFFFFFF), std::uint32_t{0xFFFFFFFF});
}
ZTH_TEST("works", "the first entries of well-known tables") {
    ZTH_EXPECT_EQ(Crc32::table[0], std::uint32_t{0});
    ZTH_EXPECT_EQ(Crc32::table[1], std::uint32_t{0x77073096});
    ZTH_EXPECT_EQ(Crc32::table[2], std::uint32_t{0xEE0E612C});
    ZTH_EXPECT_EQ(Crc32::table[255], std::uint32_t{0x2D02EF8D});
    ZTH_EXPECT_EQ(Crc32C::table[1], std::uint32_t{0xF26B8303});
    ZTH_EXPECT_EQ(Crc32Mpeg2::table[1], std::uint32_t{0x04C11DB7});
    ZTH_EXPECT_EQ(Crc32Mpeg2::table[255], std::uint32_t{0xB1F740B4});
    ZTH_EXPECT_EQ(Crc16CcittFalse::table[1], std::uint16_t{0x1021});
    ZTH_EXPECT_EQ(Crc16Arc::table[1], std::uint16_t{0xC0C1});
    ZTH_EXPECT_EQ(Crc8::table[1], std::uint8_t{0x07});
    ZTH_EXPECT_EQ(Crc8::table[128], std::uint8_t{0x89});
    ZTH_EXPECT_EQ(Crc8Rohc::table[1], std::uint8_t{0x91});
}
ZTH_TEST("works", "CRC-32 and CRC-32C of the check string") {
    ZTH_EXPECT_EQ(Crc32::compute(kCheck), std::uint32_t{0xCBF43926});
    ZTH_EXPECT_EQ(Crc32C::compute(kCheck), std::uint32_t{0xE3069283});
}
ZTH_TEST("works", "the other 32-bit algorithms: MPEG-2, BZIP2 and JAMCRC (reflected and direct, with and without a final XOR)") {
    ZTH_EXPECT_EQ(Crc32Mpeg2::compute(kCheck), std::uint32_t{0x0376E6E7});
    ZTH_EXPECT_EQ(Crc32Bzip2::compute(kCheck), std::uint32_t{0xFC891918});
    ZTH_EXPECT_EQ(Crc32Jamcrc::compute(kCheck), std::uint32_t{0x340BC6D9});
}
ZTH_TEST("works", "16-bit algorithms: CCITT-FALSE, XMODEM, GENIBUS, KERMIT, ARC and MODBUS") {
    ZTH_EXPECT_EQ(Crc16CcittFalse::compute(kCheck), std::uint16_t{0x29B1});
    ZTH_EXPECT_EQ(Crc16Xmodem::compute(kCheck), std::uint16_t{0x31C3});
    ZTH_EXPECT_EQ(Crc16Genibus::compute(kCheck), std::uint16_t{0xD64E});
    ZTH_EXPECT_EQ(Crc16Kermit::compute(kCheck), std::uint16_t{0x2189});
    ZTH_EXPECT_EQ(Crc16Arc::compute(kCheck), std::uint16_t{0xBB3D});
    ZTH_EXPECT_EQ(Crc16Modbus::compute(kCheck), std::uint16_t{0x4B37});
}
ZTH_TEST("works", "8-bit algorithms, where the register shift by 8 must vanish") {
    ZTH_EXPECT_EQ(Crc8::compute(kCheck), std::uint8_t{0xF4});
    ZTH_EXPECT_EQ(Crc8Rohc::compute(kCheck), std::uint8_t{0xD0});
}
ZTH_TEST("works", "the CRC of nothing is what the parameters say: init, reflected if needed, then the final steps") {
    ZTH_EXPECT_EQ(Crc32::compute(std::string_view("")), std::uint32_t{0});
    ZTH_EXPECT_EQ(Crc16CcittFalse::compute(std::string_view("")), std::uint16_t{0xFFFF});
    ZTH_EXPECT_EQ(Crc16Xmodem::compute(std::string_view("")), std::uint16_t{0});
    ZTH_EXPECT_EQ(Crc32Mpeg2::compute(std::string_view("")), std::uint32_t{0xFFFFFFFF});
    ZTH_EXPECT_EQ(Crc16Genibus::compute(std::string_view("")), std::uint16_t{0x0000});
    ZTH_EXPECT_EQ(Crc8Rohc::compute(std::string_view("")), std::uint8_t{0xFF});
}
ZTH_TEST("works", "a byte span and a string_view give the same answer") {
    const std::vector<std::uint8_t> v = bytes(kCheck);
    ZTH_EXPECT_EQ(Crc32::compute(std::span<const std::uint8_t>(v.data(), v.size())), Crc32::compute(std::string_view(kCheck)));
    const std::string high("\xff\x80\x00\x7f", 4);
    ZTH_EXPECT_EQ(Crc16Arc::compute(std::string_view(high)), Crc16Arc::compute(std::span<const std::uint8_t>(bytes(high))));   // bytes above 127 are not sign-extended
    ZTH_EXPECT_EQ(Crc32::compute(std::string_view(high)), ref_crc<std::uint32_t>(kCrc32, bytes(high)));
}
ZTH_TEST("works", "an incremental CRC in pieces equals the one-shot CRC, and value() can be read at any time") {
    Crc32 c;
    c.update("1234");
    ZTH_EXPECT_EQ(c.value(), Crc32::compute(std::string_view("1234")));
    ZTH_EXPECT_EQ(c.value(), Crc32::compute(std::string_view("1234")));   // reading it did not change anything
    c.update("56789");
    ZTH_EXPECT_EQ(c.value(), std::uint32_t{0xCBF43926});
    Crc16CcittFalse d;
    d.update(std::string_view("12"));
    d.update(std::string_view(""));
    d.update(std::string_view("3456789"));
    ZTH_EXPECT_EQ(d.value(), std::uint16_t{0x29B1});
}
ZTH_TEST("works", "a fresh object starts from the initial value, and reset() starts again") {
    Crc16Modbus c;
    ZTH_EXPECT_EQ(c.value(), Crc16Modbus::compute(std::string_view("")));
    c.update("garbage");
    c.reset();
    ZTH_EXPECT_EQ(c.value(), Crc16Modbus::compute(std::string_view("")));
    c.update(kCheck);
    ZTH_EXPECT_EQ(c.value(), std::uint16_t{0x4B37});
    Crc32 e;
    e.update("x");
    e.reset();
    e.update(kCheck);
    ZTH_EXPECT_EQ(e.value(), std::uint32_t{0xCBF43926});
}
ZTH_TEST("works", "the whole computation can be done at compile time") {
    constexpr std::uint32_t a = Crc32::compute(std::string_view("123456789"));
    constexpr std::uint16_t b = Crc16Arc::compute(std::string_view("123456789"));
    constexpr std::uint32_t entry = Crc32::table[1];
    ZTH_EXPECT_EQ(a, std::uint32_t{0xCBF43926});
    ZTH_EXPECT_EQ(b, std::uint16_t{0xBB3D});
    ZTH_EXPECT_EQ(entry, std::uint32_t{0x77073096});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "unusual parameter sets (mixed reflection, asymmetric init and xor) agree with an independent bit-by-bit model") {
    zth::Lcg rng(2024);
    std::string why;
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint8_t, kM1>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint8_t, kM2>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint16_t, kM3>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint16_t, kM4>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint32_t, kM5>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint32_t, kM6>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint32_t, kM7>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
}
ZTH_TEST("robust", "all the standard algorithms agree with the independent model on random data") {
    zth::Lcg rng(5);
    std::string why;
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint8_t, kCrc8>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint8_t, kCrc8Rohc>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint16_t, kCrc16CcittFalse>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint16_t, kCrc16Genibus>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint16_t, kCrc16Modbus>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint32_t, kCrc32>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint32_t, kCrc32C>(rng, why)));
    ZTH_EXPECT_TRUE((agrees_with_reference<std::uint32_t, kCrc32Bzip2>(rng, why)));
    ZTH_EXPECT_EQ(why, std::string());
}
ZTH_TEST("robust", "any way of cutting a message into pieces gives the same CRC") {
    zth::Lcg rng(77);
    for (int round = 0; round < 200; ++round) {
        std::vector<std::uint8_t> data(static_cast<std::size_t>(rng.below(300)));
        for (auto& b : data) b = static_cast<std::uint8_t>(rng.below(256));
        const std::uint32_t whole = Crc32::compute(std::span<const std::uint8_t>(data.data(), data.size()));
        Crc32 c;
        Crc16Genibus g;
        std::size_t pos = 0;
        while (pos < data.size()) {
            const std::size_t n = std::min<std::size_t>(data.size() - pos, static_cast<std::size_t>(rng.below(20)));
            c.update(std::span<const std::uint8_t>(data.data() + pos, n));
            g.update(std::span<const std::uint8_t>(data.data() + pos, n));
            pos += n;
            (void)c.value();   // peeking must never disturb the state
        }
        if (c.value() != whole || g.value() != Crc16Genibus::compute(std::span<const std::uint8_t>(data.data(), data.size()))) {
            zth::fail("a chunked CRC differs from the one-shot CRC for a message of " + std::to_string(data.size()) + " bytes");
            return;
        }
    }
}
ZTH_TEST("robust", "every single-bit error is detected (a property of every CRC with a proper polynomial)") {
    zth::Lcg rng(9);
    std::vector<std::uint8_t> data(64);
    for (auto& b : data) b = static_cast<std::uint8_t>(rng.below(256));
    const std::uint32_t good = Crc32::compute(std::span<const std::uint8_t>(data.data(), data.size()));
    const std::uint16_t good16 = Crc16CcittFalse::compute(std::span<const std::uint8_t>(data.data(), data.size()));
    for (std::size_t i = 0; i < data.size() * 8; ++i) {
        data[i / 8] = static_cast<std::uint8_t>(data[i / 8] ^ (1u << (i % 8)));
        if (Crc32::compute(std::span<const std::uint8_t>(data.data(), data.size())) == good || Crc16CcittFalse::compute(std::span<const std::uint8_t>(data.data(), data.size())) == good16) {
            zth::fail("flipping bit " + std::to_string(i) + " was not detected");
            return;
        }
        data[i / 8] = static_cast<std::uint8_t>(data[i / 8] ^ (1u << (i % 8)));
    }
}
ZTH_TEST("robust", "the table of every algorithm matches the independent model entry by entry") {
    // table[i] must equal the register after feeding the single byte i into a zero register (in the direction of that algorithm)
    for (unsigned i = 0; i < 256; ++i) {
        const std::vector<std::uint8_t> one{static_cast<std::uint8_t>(i)};
        const CrcParams<std::uint32_t> raw{kCrc32.poly, 0, true, false, 0};   // "reflected input, register unreflected at the end"
        // CRC-32 table entry i is the reflection of the direct-form register after one byte (reflected input): compare via ref_crc
        const std::uint32_t expect = ref_reflect<std::uint32_t>(ref_crc<std::uint32_t>(raw, one), 32);
        if (Crc32::table[i] != expect) {
            zth::fail("CRC-32 table[" + std::to_string(i) + "] is wrong");
            return;
        }
        const CrcParams<std::uint16_t> raw16{kCrc16CcittFalse.poly, 0, false, false, 0};
        if (Crc16CcittFalse::table[i] != ref_crc<std::uint16_t>(raw16, one)) {
            zth::fail("CRC-16/CCITT table[" + std::to_string(i) + "] is wrong");
            return;
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "no heap: neither the table nor a computation allocates") {
    zth::mem::Scope scope;
    std::uint32_t acc = 0;
    std::vector<std::uint8_t> big;   // allocated by the test, before the scope's reading is taken
    const std::size_t before = 0;
    (void)before;
    for (int i = 0; i < 1000; ++i) acc ^= Crc32::compute(std::string_view("a short message that is not in the small string buffer, honest, it is long enough"));
    Crc32 c;
    for (int i = 0; i < 1000; ++i) c.update("data");
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(acc == 0 || acc != 1);
}
ZTH_TEST("fast", "the object is only its register (the table is shared and static)") {
    ZTH_EXPECT_EQ(sizeof(Crc32), sizeof(std::uint32_t));
    ZTH_EXPECT_EQ(sizeof(Crc16Arc), sizeof(std::uint16_t));
    ZTH_EXPECT_EQ(sizeof(Crc8), sizeof(std::uint8_t));
    ZTH_EXPECT_EQ(sizeof(Crc32::table), std::size_t{256 * 4});
}
ZTH_TEST_TIMEOUT("fast", "a few megabytes of data in one go", 20) {
    std::vector<std::uint8_t> data(8 * 1024 * 1024);
    for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i * 31 + 7);
    const std::uint32_t a = Crc32::compute(std::span<const std::uint8_t>(data.data(), data.size()));
    Crc32 c;
    for (std::size_t pos = 0; pos < data.size(); pos += 4096) c.update(std::span<const std::uint8_t>(data.data() + pos, 4096));
    ZTH_EXPECT_EQ(c.value(), a);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("crc.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("crc.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "firmware style: no standard containers except std::array and span, no heap, no streams") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("crc.hpp"));
    for (const char* word : {"vector", "map", "unordered_map", "string", "iostream", "sstream", "function", "unique_ptr", "shared_ptr", "malloc"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the table is a static constexpr member produced by a constexpr function") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("crc.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "constexpr").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "make_table").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "table").empty());
}
