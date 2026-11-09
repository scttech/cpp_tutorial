#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "packet.hpp"

// FUZZ the parser in packet.hpp. A fuzz test feeds a function thousands of unusual inputs and checks that it never misbehaves. Two things
// tell you that it did: the SANITIZERS (this lesson builds with -fsanitize=address,undefined: an out-of-bounds read, an overflow or a leak ends
// the program with an error), and YOUR OWN CHECKS (a "property" that must always hold, for example: what you encode, you can decode).
//
// Press "Run Tests": your program is built and run against the real parser (it must finish without a sanitizer report and without
// aborting), and then against several BROKEN copies of the parser: your fuzz test must make every one of them crash, hang or abort.
// A broken parser that survives your fuzzing is listed by name with a hint about the kind of input you did not try.
//
// Rules of the game:
//   * Deterministic: use a FIXED seed (a failure must be reproducible), never std::random_device or the clock.
//   * Exit status: main returns 0 when all is well. A failed property must stop the program: std::abort() or return 1.
//   * Mind the buffer: the sanitizer only notices a read past the end if the buffer ends exactly where the input does.
//   * The Clean tier also wants: packets made with make_packet (structure-aware inputs), a property check, no compiler warnings.

namespace {

std::mt19937 rng(12345);  // a fixed seed

}  // namespace

int main() {
    // TODO: fuzz parse(). For the random bytes, call it on a few thousand random buffers of random length.
    for (int i = 0; i < 100; ++i) {
        std::vector<std::uint8_t> input(rng() % 8);
        for (auto& b : input) b = static_cast<std::uint8_t>(rng());
        parse(input.data(), input.size());
    }
    std::puts("fuzzing finished");
    return 0;
}
