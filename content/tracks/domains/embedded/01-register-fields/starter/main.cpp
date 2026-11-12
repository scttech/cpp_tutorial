// A UART control register described with typed fields; every access to the (simulated) hardware is counted. Read-only. Press Run.
#include <cstdint>
#include <cstdio>

#include "registers.hpp"

using Ctrl = regs::Register<std::uint32_t>;
using Enable = regs::Field<std::uint32_t, 0, 1>;
using Parity = regs::Field<std::uint32_t, 1, 2>;   // 0 none, 1 odd, 2 even
using StopBits = regs::Field<std::uint32_t, 3, 1>;
using Baud = regs::Field<std::uint32_t, 8, 8>;

int main() {
    Ctrl ctrl(0x0000'0000);
    ctrl.modify(regs::set<Enable>(1), regs::set<Parity>(2), regs::set<Baud>(0x4D));   // ONE read and ONE write for all three fields
    std::printf("CTRL = 0x%08X after %zu read(s) and %zu write(s)\n", ctrl.peek(), ctrl.reads(), ctrl.writes());
    std::printf("parity field = %u, baud field = 0x%X\n", ctrl.read_field<Parity>(), ctrl.read_field<Baud>());

    regs::Register<std::uint32_t> status(0b1011);   // interrupt flags: writing a 1 CLEARS a flag, writing a 0 changes nothing
    status.write_one_to_clear(0b0010);
    std::printf("STATUS = 0b%u%u%u%u (reads: %zu, writes: %zu)\n", (status.peek() >> 3) & 1, (status.peek() >> 2) & 1, (status.peek() >> 1) & 1, status.peek() & 1, status.reads(), status.writes());
}
