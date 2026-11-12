// MUTANT: reflect forgets one bit
#pragma once
// A reflected or direct table algorithm chosen by P.reflect_in; the table is a static constexpr member built by a constexpr function.
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
    T result = 0;
    for (unsigned i = 0; i + 1 < std::numeric_limits<T>::digits; ++i) {
        result = static_cast<T>((result << 1) | (value & 1));
        value = static_cast<T>(value >> 1);
    }
    return result;
}

template <class T, CrcParams<T> P>
class Crc {
public:
    static constexpr unsigned kBits = std::numeric_limits<T>::digits;

    static constexpr std::array<T, 256> make_table() {
        std::array<T, 256> t{};
        for (unsigned i = 0; i < 256; ++i) {
            if (P.reflect_in) {
                T v = static_cast<T>(i);
                for (int bit = 0; bit < 8; ++bit) v = (v & 1) ? static_cast<T>((v >> 1) ^ reflect(P.poly)) : static_cast<T>(v >> 1);
                t[i] = v;
            } else {
                const T top = static_cast<T>(T{1} << (kBits - 1));
                T v = static_cast<T>(static_cast<T>(i) << (kBits - 8));
                for (int bit = 0; bit < 8; ++bit) v = (v & top) ? static_cast<T>((v << 1) ^ P.poly) : static_cast<T>(v << 1);
                t[i] = v;
            }
        }
        return t;
    }
    static constexpr std::array<T, 256> table = make_table();

    constexpr Crc() = default;

    constexpr void update(std::span<const std::uint8_t> data) {
        for (std::uint8_t b : data) {
            if constexpr (P.reflect_in) {
                state_ = static_cast<T>(table[(state_ ^ b) & 0xFF] ^ (state_ >> 8));
            } else {
                state_ = static_cast<T>(table[((state_ >> (kBits - 8)) ^ b) & 0xFF] ^ (state_ << 8));
            }
        }
    }
    constexpr void update(std::string_view text) {
        for (char c : text) update_byte(static_cast<std::uint8_t>(c));
    }

    constexpr T value() const {
        T r = state_;
        if (P.reflect_in != P.reflect_out) r = reflect(r);
        return static_cast<T>(r ^ P.xor_out);
    }
    constexpr void reset() { state_ = start(); }

    static constexpr T compute(std::span<const std::uint8_t> data) {
        Crc c;
        c.update(data);
        return c.value();
    }
    static constexpr T compute(std::string_view text) {
        Crc c;
        c.update(text);
        return c.value();
    }

private:
    static constexpr T start() { return P.reflect_in ? reflect(P.init) : P.init; }

    constexpr void update_byte(std::uint8_t b) {
        const std::uint8_t one[1] = {b};
        update(std::span<const std::uint8_t>(one, 1));
    }

    T state_ = start();
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
