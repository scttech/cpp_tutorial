// Frame checksums for a serial link: the CRC table is computed by the compiler, so it costs no RAM and no start-up time. Read-only. Press Run.
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "crc.hpp"

int main() {
    constexpr std::string_view check = "123456789";
    constexpr std::uint32_t c32 = crc::Crc32::compute(check);   // evaluated by the compiler
    std::printf("CRC-32 of \"123456789\"      = 0x%08X (expected CBF43926)\n", c32);
    std::printf("CRC-32C                     = 0x%08X (expected E3069283)\n", crc::Crc32C::compute(check));
    std::printf("CRC-16/CCITT-FALSE          = 0x%04X (expected 29B1)\n", crc::Crc16CcittFalse::compute(check));
    std::printf("CRC-16/MODBUS               = 0x%04X (expected 4B37)\n", crc::Crc16Modbus::compute(check));
    std::printf("CRC-8                       = 0x%02X (expected F4)\n", crc::Crc8::compute(check));

    crc::Crc32 running;   // a stream arriving in pieces
    running.update("1234");
    running.update("56789");
    std::printf("in two pieces               = 0x%08X\n", running.value());
    std::printf("table[1] of CRC-32          = 0x%08X, table[255] = 0x%08X\n", crc::Crc32::table[1], crc::Crc32::table[255]);
}
