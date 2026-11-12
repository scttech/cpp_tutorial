#pragma once
// READ-ONLY. The parameters of a CRC algorithm in the "Rocksoft model" used by every CRC catalogue (and by the CRC RevEng database).
#include <cstdint>

namespace crc {

template <class T>
struct CrcParams {
    T poly;          // the generator polynomial, without its top bit (CRC-32: 0x04C11DB7)
    T init;          // the initial register value
    bool reflect_in;   // each input byte is processed least significant bit first
    bool reflect_out;  // the final register is bit-reversed before the final XOR
    T xor_out;       // XORed into the result
};

// Standard algorithms. Each one's "check" value is the CRC of the ASCII text 123456789.
inline constexpr CrcParams<std::uint8_t> kCrc8{0x07, 0x00, false, false, 0x00};              // check 0xF4
inline constexpr CrcParams<std::uint8_t> kCrc8Rohc{0x07, 0xFF, true, true, 0x00};            // check 0xD0
inline constexpr CrcParams<std::uint16_t> kCrc16CcittFalse{0x1021, 0xFFFF, false, false, 0x0000};  // check 0x29B1
inline constexpr CrcParams<std::uint16_t> kCrc16Xmodem{0x1021, 0x0000, false, false, 0x0000};      // check 0x31C3
inline constexpr CrcParams<std::uint16_t> kCrc16Genibus{0x1021, 0xFFFF, false, false, 0xFFFF};     // check 0xD64E
inline constexpr CrcParams<std::uint16_t> kCrc16Kermit{0x1021, 0x0000, true, true, 0x0000};        // check 0x2189
inline constexpr CrcParams<std::uint16_t> kCrc16Arc{0x8005, 0x0000, true, true, 0x0000};           // check 0xBB3D
inline constexpr CrcParams<std::uint16_t> kCrc16Modbus{0x8005, 0xFFFF, true, true, 0x0000};        // check 0x4B37
inline constexpr CrcParams<std::uint32_t> kCrc32{0x04C11DB7, 0xFFFFFFFF, true, true, 0xFFFFFFFF};      // check 0xCBF43926
inline constexpr CrcParams<std::uint32_t> kCrc32C{0x1EDC6F41, 0xFFFFFFFF, true, true, 0xFFFFFFFF};     // check 0xE3069283
inline constexpr CrcParams<std::uint32_t> kCrc32Mpeg2{0x04C11DB7, 0xFFFFFFFF, false, false, 0x00000000};  // check 0x0376E6E7
inline constexpr CrcParams<std::uint32_t> kCrc32Bzip2{0x04C11DB7, 0xFFFFFFFF, false, false, 0xFFFFFFFF}; // check 0xFC891918
inline constexpr CrcParams<std::uint32_t> kCrc32Jamcrc{0x04C11DB7, 0xFFFFFFFF, true, true, 0x00000000};   // check 0x340BC6D9

}  // namespace crc
