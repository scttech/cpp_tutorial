// MUTANT: no bang.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

// A small band. Six mistakes were made in the starter version of this file and of main.cpp, each reported by the compiler (or by a test) in a
// different way. The bodies of whistle_demo and snare_demo are in main.cpp.

inline int instruments_alive = 0;  // how many Instrument objects exist right now (the tests read it)
inline int drumheads_alive = 0;    // how many Drum objects exist: every drum has a drumhead that its destructor releases

class Instrument {
public:
    explicit Instrument(std::string name) : name_(std::move(name)) { ++instruments_alive; }
    Instrument(const Instrument& other) : name_(other.name_) { ++instruments_alive; }
    // 4. A base class that is deleted through a pointer needs a VIRTUAL destructor.
    virtual ~Instrument() { --instruments_alive; }

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

// 1. `override` must match the base signature EXACTLY: sound() is const.
class Guitar : public Instrument {
public:
    Guitar() : Instrument("guitar") {}
    std::string sound() const override { return "strum"; }
};

// 5. play(int) is what the base declares: a play(double) would only HIDE it.
class Drum : public Instrument {
public:
    explicit Drum(std::string name = "drum") : Instrument(std::move(name)) { ++drumheads_alive; }
    Drum(const Drum& other) : Instrument(other) { ++drumheads_alive; }
    ~Drum() { --drumheads_alive; }
    std::string sound() const override { return "boom"; }
    std::string play(int times) const override {
        std::string out;
        for (int i = 0; i < times; ++i) out += (i ? " " : "") + sound();
        return out;
    }
};

class SnareDrum final : public Drum {
public:
    SnareDrum() : Drum("snare") {}
    std::string sound() const override { return "crack"; }
};

// 2. A class that does not implement every pure virtual function stays abstract: no Piano can be created.
class Piano : public Instrument {
public:
    Piano() : Instrument("piano") {}
    std::string sound() const override { return "plink"; }
};

// 3. `class Whistle : Instrument` would inherit PRIVATELY: write public when a Whistle IS-A Instrument.
class Whistle : public Instrument {
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

// 6. The sound of a drum: take it by REFERENCE. By value the parameter is a sliced copy: a SnareDrum becomes a plain Drum.
inline std::string hear(const Drum& drum) { return drum.sound(); }

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
