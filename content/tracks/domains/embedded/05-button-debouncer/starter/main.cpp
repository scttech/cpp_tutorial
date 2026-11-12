// Raw switch samples (with contact bounce) in, clean events out. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "button.hpp"
#include "types.hpp"

int main() {
    btn::Button b({/*debounce*/ 2, /*long press*/ 5, /*double click*/ 6});
    const std::string script = "0101001111011110000110011000000000000111111111100000";
    std::printf("raw   %s\n", script.c_str());
    std::string out;
    for (char c : script) {
        const std::uint8_t ev = b.update(c == '1');
        std::string s;
        if (ev & btn::kPressed) s += 'P';
        if (ev & btn::kLongPress) s += 'L';
        if (ev & btn::kReleased) s += 'R';
        if (ev & btn::kClick) s += 'C';
        if (ev & btn::kDoubleClick) s += 'D';
        out += s.empty() ? "." : (s.size() == 1 ? s : "[" + s + "]");
    }
    std::printf("event %s\n", out.c_str());
    std::printf("P pressed  R released  C click  D double click  L long press   (final state: %s)\n", b.pressed() ? "pressed" : "released");
}
