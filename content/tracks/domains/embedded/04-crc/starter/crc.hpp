#pragma once
// YOUR CRC ENGINE: any width-8/16/32 CRC from its Rocksoft parameters (see types.hpp), with a 256-entry lookup table built AT COMPILE TIME.
//
//   Crc<T, P>                 T is uint8_t, uint16_t or uint32_t; P is a CrcParams<T> (a constexpr value used as a template argument)
//   Crc<T,P>::table           std::array<T, 256>, `static constexpr`, computed by a constexpr function: no run-time initialisation, no RAM
//   Crc<T,P>::compute(data)   the CRC of a byte span or a string_view in one call (constexpr)
//   Crc<T,P> c;  c.update(data) ... c.value()   the same, in pieces. value() does NOT change the state (you may keep updating afterwards).
//                c.reset() starts again.
//
// THE ALGORITHM (table-driven; the register is kept in the direction the bytes are fed):
//   reflect_in == true  : register r starts as reflect(init). Per byte b:  r = table[(r ^ b) & 0xFF] ^ (r >> 8)
//                         table[i]: start with i, 8 times: if (v & 1) v = (v >> 1) ^ reflect(poly) else v >>= 1
//   reflect_in == false : register r starts as init. Per byte b:           r = table[((r >> (bits - 8)) ^ b) & 0xFF] ^ (r << 8)
//                         table[i]: start with i << (bits - 8), 8 times: if (v & top bit) v = (v << 1) ^ poly else v <<= 1
//   result: if (reflect_in != reflect_out) r = reflect(r) (the register is in the "wrong" direction); then r ^ xor_out.
//   reflect(x) reverses all the bits of x (as a T). Empty input gives the same as the formula with no bytes (CRC-32 of "" is 0).
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "types.hpp"

namespace crc {

template <class T>
constexpr T reflect(T value) {
    (void)value;
    return 0;  // TODO: reverse the bits
}

template <class T, CrcParams<T> P>
class Crc {
public:
    static constexpr unsigned kBits = std::numeric_limits<T>::digits;

    static constexpr std::array<T, 256> make_table() {
        std::array<T, 256> t{};
        return t;  // TODO
    }
    static constexpr std::array<T, 256> table = make_table();

    constexpr Crc() = default;

    constexpr void update(std::span<const std::uint8_t> data) { (void)data; }  // TODO
    constexpr void update(std::string_view text) { (void)text; }               // TODO
    constexpr T value() const { return 0; }                                    // TODO
    constexpr void reset() {}                                                  // TODO

    static constexpr T compute(std::span<const std::uint8_t> data) {
        (void)data;
        return 0;  // TODO
    }
    static constexpr T compute(std::string_view text) {
        (void)text;
        return 0;  // TODO
    }

private:
    T state_ = 0;  // TODO: the right start value
};

using Crc8 = Crc<std::uint8_t, kCrc8>;
using Crc8Rohc = Crc<std::uint8_t, kCrc8Rohc>;
using Crc16CcittFalse = Crc<std::uint16_t, kCrc16CcittFalse>;
using Crc16Xmodem = Crc<std::uint16_t, kCrc16Xmodem>;
using Crc16Genibus = Crc<std::uint16_t, kCrc16Genibus>;
using Crc16Kermit = Crc<std::uint16_t, kCrc16Kermit>;
using Crc16Arc = Crc<std::uint16_t, kCrc16Arc>;
using Crc16Modbus = Crc<std::uint16_t, kCrc16Modbus>;
using Crc32 = Crc<std::uint32_t, kCrc32>;
using Crc32C = Crc<std::uint32_t, kCrc32C>;
using Crc32Mpeg2 = Crc<std::uint32_t, kCrc32Mpeg2>;
using Crc32Bzip2 = Crc<std::uint32_t, kCrc32Bzip2>;
using Crc32Jamcrc = Crc<std::uint32_t, kCrc32Jamcrc>;

}  // namespace crc
