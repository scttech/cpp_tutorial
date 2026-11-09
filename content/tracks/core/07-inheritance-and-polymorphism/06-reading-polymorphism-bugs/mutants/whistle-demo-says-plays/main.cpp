// MUTANT: wrong format.
#include <iostream>
#include <string>

#include "band.hpp"

std::string describe(const Instrument& instrument) { return instrument.name() + " plays " + instrument.sound(); }

// 3. A Whistle must be usable as an Instrument: public inheritance (see band.hpp).
std::string whistle_demo() {
    Whistle whistle;
    return describe(whistle);
}

// 6. hear takes a reference now, so the SnareDrum is not sliced.
std::string snare_demo() {
    SnareDrum snare;
    return hear(snare);
}

int main() {
    std::cout << whistle_demo() << "\n";
    std::cout << "snare: " << snare_demo() << "\n";
    Band band = make_band();
    std::cout << rehearse(band, 2) << "\n";
    std::cout << "instruments alive: " << instruments_alive << "\n";
    band.clear();
    std::cout << "after clearing the band: " << instruments_alive << " (should be 0)\n";
}
