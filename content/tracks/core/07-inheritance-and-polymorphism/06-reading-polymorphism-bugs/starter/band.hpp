#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

// A small band, with six mistakes: each one is reported by the compiler (or, for two of them, only by a test) in a different way.
// The bodies of whistle_demo and snare_demo are in main.cpp. Read the FIRST message in the Problems tab, fix it, run again.

inline int instruments_alive = 0;  // how many Instrument objects exist right now (the tests read it)
inline int drumheads_alive = 0;    // how many Drum objects exist: every drum has a drumhead that its destructor releases

class Instrument {
public:
    explicit Instrument(std::string name) : name_(std::move(name)) { ++instruments_alive; }
    Instrument(const Instrument& other) : name_(other.name_) { ++instruments_alive; }
    ~Instrument() { --instruments_alive; }

    const std::string& name() const { return name_; }
    virtual std::string sound() const = 0;
    // The sound `times` times, separated by spaces ("strum strum"); nothing for times <= 0.
    virtual std::string play(int times) const {
        std::string out;
        for (int i = 0; i < times; ++i) out += (i ? " " : "") + sound();
        return out;
    }

private:
    std::string name_;
};

class Guitar : public Instrument {
public:
    Guitar() : Instrument("guitar") {}
    std::string sound() override { return "strum"; }
};

// A drum plays "boom!" for every beat: "boom! boom!".
class Drum : public Instrument {
public:
    explicit Drum(std::string name = "drum") : Instrument(std::move(name)) { ++drumheads_alive; }
    Drum(const Drum& other) : Instrument(other) { ++drumheads_alive; }
    ~Drum() { --drumheads_alive; }
    std::string sound() const override { return "boom"; }
    std::string play(double times) const {
        std::string out;
        for (int i = 0; i < times; ++i) out += (i ? " " : "") + sound() + "!";
        return out;
    }
};

class SnareDrum final : public Drum {
public:
    SnareDrum() : Drum("snare") {}
    std::string sound() const override { return "crack"; }
};

class Piano : public Instrument {
public:
    Piano() : Instrument("piano") {}
};

class Whistle : Instrument {
public:
    Whistle() : Instrument("whistle") {}
    std::string sound() const override { return "tweet"; }
};

using Band = std::vector<std::unique_ptr<Instrument>>;

// A guitar, a drum, a piano and a whistle, in that order.
inline Band make_band() {
    Band band;
    band.push_back(std::make_unique<Guitar>());
    band.push_back(std::make_unique<Drum>());
    band.push_back(std::make_unique<Piano>());
    band.push_back(std::make_unique<Whistle>());
    return band;
}

// The sound of a drum.
inline std::string hear(Drum drum) { return drum.sound(); }

// Every instrument plays `times` times; the results are joined with " | ".
inline std::string rehearse(const Band& band, int times) {
    std::string out;
    for (const auto& instrument : band) {
        if (!out.empty()) out += " | ";
        out += instrument->play(times);
    }
    return out;
}

std::string whistle_demo();  // "whistle says tweet": defined in main.cpp
std::string snare_demo();    // the sound of a SnareDrum, heard through hear(): defined in main.cpp
