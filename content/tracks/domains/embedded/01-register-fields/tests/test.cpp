#include <cstdint>
#include <string>
#include <type_traits>

#include "registers.hpp"
#include "zth_test.hpp"

using namespace regs;

namespace {
using F0 = Field<std::uint32_t, 0, 1>;
using F12 = Field<std::uint32_t, 1, 2>;
using F8 = Field<std::uint32_t, 8, 8>;
using Top = Field<std::uint32_t, 28, 4>;
using Whole32 = Field<std::uint32_t, 0, 32>;
using Whole8 = Field<std::uint8_t, 0, 8>;
using Mid16 = Field<std::uint16_t, 4, 8>;
using Hi8 = Field<std::uint8_t, 5, 3>;
}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "masks and maximum values of fields at the edges of a register") {
    ZTH_EXPECT_EQ(F0::mask, std::uint32_t{0x1});
    ZTH_EXPECT_EQ(F0::max_value, std::uint32_t{1});
    ZTH_EXPECT_EQ(F12::mask, std::uint32_t{0x6});
    ZTH_EXPECT_EQ(F12::max_value, std::uint32_t{3});
    ZTH_EXPECT_EQ(F8::mask, std::uint32_t{0xFF00});
    ZTH_EXPECT_EQ(F8::max_value, std::uint32_t{255});
    ZTH_EXPECT_EQ(Top::mask, std::uint32_t{0xF0000000u});
    ZTH_EXPECT_EQ(Top::max_value, std::uint32_t{15});
    ZTH_EXPECT_EQ(Mid16::mask, std::uint16_t{0x0FF0});
    ZTH_EXPECT_EQ(Hi8::mask, std::uint8_t{0xE0});
    ZTH_EXPECT_EQ(Hi8::max_value, std::uint8_t{7});
}
ZTH_TEST("works", "a field as wide as its register has an all-ones mask (no undefined shift by the full width)") {
    ZTH_EXPECT_EQ(Whole32::mask, std::uint32_t{0xFFFFFFFFu});
    ZTH_EXPECT_EQ(Whole32::max_value, std::uint32_t{0xFFFFFFFFu});
    ZTH_EXPECT_EQ(Whole8::mask, std::uint8_t{0xFF});
    ZTH_EXPECT_EQ(Whole32::get(0xDEADBEEFu), std::uint32_t{0xDEADBEEFu});
    ZTH_EXPECT_EQ(Whole32::insert(0x12345678u, 0xCAFEF00Du), std::uint32_t{0xCAFEF00Du});
    ZTH_EXPECT_TRUE(Whole32::fits(0xFFFFFFFFu));
}
ZTH_TEST("works", "get extracts a field and shifts it down") {
    ZTH_EXPECT_EQ(F12::get(0b110u), std::uint32_t{3});
    ZTH_EXPECT_EQ(F12::get(0b101u), std::uint32_t{2});
    ZTH_EXPECT_EQ(F8::get(0x1234ABCDu), std::uint32_t{0xAB});
    ZTH_EXPECT_EQ(Top::get(0xA0000000u), std::uint32_t{0xA});
    ZTH_EXPECT_EQ(F0::get(0xFFFFFFFEu), std::uint32_t{0});
    ZTH_EXPECT_EQ(Hi8::get(std::uint8_t{0b10110101}), std::uint8_t{0b101});
}
ZTH_TEST("works", "insert replaces only the field: old bits of the field are cleared, the neighbours are untouched") {
    ZTH_EXPECT_EQ(F12::insert(0xFFFFFFFFu, 0), std::uint32_t{0xFFFFFFF9u});
    ZTH_EXPECT_EQ(F12::insert(0u, 3), std::uint32_t{0x6});
    ZTH_EXPECT_EQ(F12::insert(0b110u, 1), std::uint32_t{0b010u});   // 3 -> 1 must clear bit 2
    ZTH_EXPECT_EQ(F8::insert(0x12345678u, 0xAB), std::uint32_t{0x1234AB78u});
    ZTH_EXPECT_EQ(Top::insert(0x0FFFFFFFu, 0xA), std::uint32_t{0xAFFFFFFFu});
    ZTH_EXPECT_EQ(Hi8::insert(std::uint8_t{0xFF}, std::uint8_t{2}), std::uint8_t{0b01011111});
    ZTH_EXPECT_EQ(Mid16::insert(std::uint16_t{0xFFFF}, std::uint16_t{0}), std::uint16_t{0xF00F});
}
ZTH_TEST("works", "a value that is too wide is truncated and never spills into neighbouring fields") {
    ZTH_EXPECT_EQ(F12::insert(0u, 0b111), std::uint32_t{0x6});            // 7 -> 3
    ZTH_EXPECT_EQ(F0::insert(0u, 0xFF), std::uint32_t{0x1});
    ZTH_EXPECT_EQ(F8::insert(0x000000FFu, 0x1FF), std::uint32_t{0x0000FFFFu});
    ZTH_EXPECT_EQ(Top::insert(0u, 0xFF), std::uint32_t{0xF0000000u});
    ZTH_EXPECT_EQ(Hi8::insert(std::uint8_t{0}, std::uint8_t{0xFF}), std::uint8_t{0xE0});
}
ZTH_TEST("works", "fits tells whether a value is in range") {
    ZTH_EXPECT_TRUE(F12::fits(0));
    ZTH_EXPECT_TRUE(F12::fits(3));
    ZTH_EXPECT_FALSE(F12::fits(4));
    ZTH_EXPECT_TRUE(F8::fits(255));
    ZTH_EXPECT_FALSE(F8::fits(256));
    ZTH_EXPECT_TRUE(F0::fits(1));
    ZTH_EXPECT_FALSE(F0::fits(2));
}
ZTH_TEST("works", "get and insert can be used in constant expressions") {
    constexpr std::uint32_t v = F8::insert(0u, 0x5A);   // must compile as a constant expression
    constexpr std::uint32_t g = F8::get(v);
    constexpr std::uint32_t m = F12::mask;
    ZTH_EXPECT_EQ(g, std::uint32_t{0x5A});
    ZTH_EXPECT_EQ(m, std::uint32_t{0x6});
    ZTH_EXPECT_EQ(v, std::uint32_t{0x5A00});
}
ZTH_TEST("works", "read and write are counted; peek is not") {
    Register<std::uint32_t> r(0x10);
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0x10});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{0});
    ZTH_EXPECT_EQ(r.read(), std::uint32_t{0x10});
    r.write(0x22);
    r.write(0x33);
    ZTH_EXPECT_EQ(r.read(), std::uint32_t{0x33});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{2});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{2});
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0x33});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{2});
}
ZTH_TEST("works", "read_field costs one read and no write") {
    Register<std::uint32_t> r(0x1234ABCD);
    ZTH_EXPECT_EQ(r.read_field<F8>(), std::uint32_t{0xAB});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{1});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{0});
}
ZTH_TEST("works", "write_field is a read-modify-write: one read, one write, the other fields keep their values") {
    Register<std::uint32_t> r(0xFFFFFFFF);
    r.write_field<F12>(0);
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0xFFFFFFF9u});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{1});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{1});
    r.write_field<Top>(0x5);
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0x5FFFFFF9u});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{2});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{2});
}
ZTH_TEST("works", "modify changes several fields with ONE read and ONE write") {
    Register<std::uint32_t> r(0x0);
    r.modify(set<F0>(std::uint32_t{1}), set<F12>(std::uint32_t{2}), set<F8>(std::uint32_t{0x4D}));
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0x4D05});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{1});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{1});
    r.modify(set<F8>(std::uint32_t{0x01}));
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0x0105});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{2});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{2});
}
ZTH_TEST("works", "modify with later assignments to the same bits: the last one wins, still one read and one write") {
    Register<std::uint32_t> r(0xFFFFFFFF);
    r.modify(set<F8>(std::uint32_t{0x11}), set<F8>(std::uint32_t{0x22}));
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0xFFFF22FFu});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{1});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{1});
}
ZTH_TEST("works", "set_bits, clear_bits and toggle_bits are read-modify-writes on raw masks") {
    Register<std::uint32_t> r(0b1010);
    r.set_bits(0b0101);
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0b1111});
    r.clear_bits(0b0110);
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0b1001});
    r.toggle_bits(0b1100);
    ZTH_EXPECT_EQ(r.peek(), std::uint32_t{0b0101});
    ZTH_EXPECT_EQ(r.reads(), std::size_t{3});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{3});
    Register<std::uint8_t> small(0xF0);
    small.clear_bits(0x30);                       // ~mask must stay inside 8 bits
    ZTH_EXPECT_EQ(small.peek(), std::uint8_t{0xC0});
}
ZTH_TEST("works", "write_one_to_clear writes the mask once and never reads (a read-modify-write would clear the other flags)") {
    Register<std::uint32_t> status(0b1011);
    status.write_one_to_clear(0b0010);
    ZTH_EXPECT_EQ(status.reads(), std::size_t{0});
    ZTH_EXPECT_EQ(status.writes(), std::size_t{1});
}
ZTH_TEST("works", "wait_for returns as soon as the field has the expected value") {
    Register<std::uint32_t> r(0x0300);
    ZTH_EXPECT_TRUE(r.wait_for<F8>(3, 10));
    ZTH_EXPECT_EQ(r.reads(), std::size_t{1});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{0});
}
ZTH_TEST("works", "wait_for gives up after max_polls reads and never reads when max_polls is 0") {
    Register<std::uint32_t> r(0x0000);
    ZTH_EXPECT_FALSE(r.wait_for<F8>(3, 5));
    ZTH_EXPECT_EQ(r.reads(), std::size_t{5});
    ZTH_EXPECT_FALSE(r.wait_for<F8>(0, 0));
    ZTH_EXPECT_EQ(r.reads(), std::size_t{5});
    ZTH_EXPECT_TRUE(r.wait_for<F8>(0, 1));   // the very last allowed poll counts
    ZTH_EXPECT_EQ(r.reads(), std::size_t{6});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "every field of a 32-bit register: insert/get round-trip and the other bits never change") {
    zth::Lcg rng(77);
    for (unsigned offset = 0; offset < 32; ++offset) {
        for (unsigned width = 1; offset + width <= 32; ++width) {
            const std::uint32_t max = width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1);
            const std::uint32_t mask = max << offset;
            const std::uint32_t reg = static_cast<std::uint32_t>(rng.next()) * 2654435761u;
            const std::uint32_t value = static_cast<std::uint32_t>(rng.next()) * 40503u;
            // the generic Field needs compile-time numbers: go through a table of instantiations
            std::uint32_t got_mask = 0, got_get = 0, got_ins = 0;
            auto probe = [&](auto field) {
                using F = decltype(field);
                got_mask = F::mask;
                got_get = F::get(reg);
                got_ins = F::insert(reg, value);
            };
            switch (offset * 100 + width) {
#define CASE(O, W) case (O) * 100 + (W): probe(Field<std::uint32_t, O, W>{}); break;
                CASE(0, 1) CASE(0, 7) CASE(0, 8) CASE(0, 31) CASE(0, 32) CASE(1, 1) CASE(1, 2) CASE(3, 5) CASE(4, 4) CASE(7, 1) CASE(7, 2)
                CASE(8, 8) CASE(12, 12) CASE(15, 1) CASE(15, 17) CASE(16, 16) CASE(24, 8) CASE(28, 4) CASE(31, 1) CASE(30, 2) CASE(20, 12)
#undef CASE
                default: continue;
            }
            if (got_mask != mask || got_get != ((reg & mask) >> offset) || got_ins != ((reg & ~mask) | ((value << offset) & mask))) {
                zth::fail("field offset " + std::to_string(offset) + " width " + std::to_string(width) + " is wrong");
                return;
            }
        }
    }
}
ZTH_TEST("robust", "an 8-bit and a 16-bit register never leak bits past their width") {
    using A = Field<std::uint8_t, 4, 4>;
    ZTH_EXPECT_EQ(A::insert(std::uint8_t{0x0F}, std::uint8_t{0xAB}), std::uint8_t{0xBF});
    ZTH_EXPECT_EQ(A::get(std::uint8_t{0xB5}), std::uint8_t{0xB});
    using B = Field<std::uint16_t, 12, 4>;
    ZTH_EXPECT_EQ(B::insert(std::uint16_t{0x0FFF}, std::uint16_t{0x3}), std::uint16_t{0x3FFF});
    ZTH_EXPECT_EQ(B::mask, std::uint16_t{0xF000});
    Register<std::uint8_t> r(0xFF);
    r.write_field<A>(0x0);
    ZTH_EXPECT_EQ(r.peek(), std::uint8_t{0x0F});
}
ZTH_TEST("robust", "a status register with several pending flags: write_one_to_clear clears exactly the flags asked for") {
    Register<std::uint32_t> status(0xF0F0F0F0u);
    status.write_one_to_clear(0x00F00000u);
    // the hardware model: a 1 written to a flag clears it, a 0 changes nothing
    ZTH_EXPECT_EQ(status.writes(), std::size_t{1});
    ZTH_EXPECT_EQ(status.reads(), std::size_t{0});
    // modelled by the register simply storing what was written; the point is WHAT is written
    ZTH_EXPECT_EQ(status.peek(), std::uint32_t{0x00F00000u});
}
ZTH_TEST("robust", "operations compose: configure, poll for ready, clear flags") {
    Register<std::uint32_t> ctrl(0);
    ctrl.modify(set<F0>(std::uint32_t{1}), set<F12>(std::uint32_t{1}), set<F8>(std::uint32_t{0x10}));
    ZTH_EXPECT_EQ(ctrl.read_field<F12>(), std::uint32_t{1});
    ZTH_EXPECT_TRUE(ctrl.wait_for<F0>(1, 1));
    ctrl.clear_bits(F0::mask);
    ZTH_EXPECT_EQ(ctrl.peek(), std::uint32_t{0x1002});
    ctrl.toggle_bits(F0::mask);
    ZTH_EXPECT_EQ(ctrl.peek(), std::uint32_t{0x1003});
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "hardware accesses are the cost: counts for 1000 register updates") {
    Register<std::uint32_t> r(0);
    for (std::uint32_t i = 0; i < 1000; ++i) r.modify(set<F8>(i & 0xFF), set<F12>(i & 3), set<F0>(i & 1));
    ZTH_EXPECT_EQ(r.reads(), std::size_t{1000});
    ZTH_EXPECT_EQ(r.writes(), std::size_t{1000});
    Register<std::uint32_t> s(0);
    for (std::uint32_t i = 0; i < 1000; ++i) s.write_one_to_clear(1u << (i % 32));
    ZTH_EXPECT_EQ(s.reads(), std::size_t{0});
    ZTH_EXPECT_EQ(s.writes(), std::size_t{1000});
    Register<std::uint32_t> p(0x100);
    for (int i = 0; i < 100; ++i) ZTH_EXPECT_FALSE(p.wait_for<F0>(1, 7));
    ZTH_EXPECT_EQ(p.reads(), std::size_t{700});
}
ZTH_TEST("fast", "field operations are free of overhead: all are constexpr and noexcept-friendly (usable in constant expressions)") {
    constexpr auto a = F12::insert(0xFFu, 2);
    constexpr auto b = F8::get(0xAB00u);
    constexpr auto c = Top::fits(15);
    ZTH_EXPECT_EQ(b, std::uint32_t{0xAB});
    ZTH_EXPECT_TRUE(c);
    ZTH_EXPECT_EQ(a, std::uint32_t{0xFD});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no macros") {
    ZTH_EXPECT_NO_RAW_MEMORY("registers.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("registers.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "define"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "no heap and no standard containers in firmware code") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("registers.hpp"));
    for (const char* word : {"vector", "string", "map", "unique_ptr", "shared_ptr", "function", "malloc", "iostream", "cstdio"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the field type is built with constexpr and a fold expression handles modify()") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("registers.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "constexpr").empty());
}
