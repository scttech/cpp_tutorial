#define ZTH_TRACK_ALLOCATIONS
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "band.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;

// A drum written by the TEST that overrides sound(): hear() must reach it, which a by-value (sliced) parameter cannot.
struct Timpani final : Drum {
    Timpani() : Drum("timpani") {}
    S sound() const override { return "rumble"; }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "each instrument has its sound, also through an Instrument reference") {
    Guitar g;
    Drum d;
    Piano p;
    Whistle w;
    const Instrument& ig = g;
    const Instrument& id = d;
    const Instrument& ip = p;
    const Instrument& iw = w;
    ZTH_EXPECT_EQ(ig.sound(), S("strum"));
    ZTH_EXPECT_EQ(id.sound(), S("boom"));
    ZTH_EXPECT_EQ(ip.sound(), S("plink"));
    ZTH_EXPECT_EQ(iw.sound(), S("tweet"));
}
ZTH_TEST("works", "names") {
    ZTH_EXPECT_EQ(Guitar().name(), S("guitar"));
    ZTH_EXPECT_EQ(Drum().name(), S("drum"));
    ZTH_EXPECT_EQ(SnareDrum().name(), S("snare"));
    ZTH_EXPECT_EQ(Piano().name(), S("piano"));
    ZTH_EXPECT_EQ(Whistle().name(), S("whistle"));
}
ZTH_TEST("works", "SnareDrum sounds like a snare, through a Drum reference too") {
    SnareDrum s;
    const Drum& d = s;
    ZTH_EXPECT_EQ(d.sound(), S("crack"));
}
ZTH_TEST("works", "an Instrument plays its sound, repeated and separated by spaces") {
    Guitar g;
    ZTH_EXPECT_EQ(g.play(1), S("strum"));
    ZTH_EXPECT_EQ(g.play(3), S("strum strum strum"));
    ZTH_EXPECT_EQ(g.play(0), S(""));
    ZTH_EXPECT_EQ(g.play(-2), S(""));
}
ZTH_TEST("works", "a Drum plays with an exclamation mark, ALSO through an Instrument reference (play(int) overrides)") {
    Drum d;
    const Instrument& i = d;
    ZTH_EXPECT_EQ(i.play(2), S("boom! boom!"));
    ZTH_EXPECT_EQ(d.play(1), S("boom!"));
    ZTH_EXPECT_EQ(i.play(0), S(""));
}
ZTH_TEST("works", "a SnareDrum plays its own sound with the drum rhythm") {
    SnareDrum s;
    const Instrument& i = s;
    ZTH_EXPECT_EQ(i.play(2), S("crack! crack!"));
}
ZTH_TEST("works", "make_band: a guitar, a drum, a piano and a whistle") {
    const Band band = make_band();
    ZTH_EXPECT_EQ(band.size(), std::size_t{4});
    ZTH_EXPECT_EQ(band[0]->name(), S("guitar"));
    ZTH_EXPECT_EQ(band[1]->name(), S("drum"));
    ZTH_EXPECT_EQ(band[2]->name(), S("piano"));
    ZTH_EXPECT_EQ(band[3]->name(), S("whistle"));
}
ZTH_TEST("works", "rehearse joins what each instrument plays") {
    const Band band = make_band();
    ZTH_EXPECT_EQ(rehearse(band, 2), S("strum strum | boom! boom! | plink plink | tweet tweet"));
    ZTH_EXPECT_EQ(rehearse(band, 1), S("strum | boom! | plink | tweet"));
}
ZTH_TEST("works", "rehearse of an empty band is empty") {
    ZTH_EXPECT_EQ(rehearse(Band{}, 3), S(""));
}
ZTH_TEST("works", "whistle_demo (main.cpp): a Whistle is used as an Instrument") {
    ZTH_EXPECT_EQ(whistle_demo(), S("whistle says tweet"));
}
ZTH_TEST("works", "snare_demo (main.cpp): the snare is heard as a snare, not as a plain drum") {
    ZTH_EXPECT_EQ(snare_demo(), S("crack"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "Instrument is abstract; every instrument of the band is concrete") {
    ZTH_EXPECT_TRUE(std::is_abstract_v<Instrument>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<Guitar>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<Drum>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<Piano>);
    ZTH_EXPECT_FALSE(std::is_abstract_v<Whistle>);
}
ZTH_TEST("robust", "every instrument IS an Instrument (public inheritance)") {
    ZTH_EXPECT_TRUE((std::is_convertible_v<Guitar*, Instrument*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<Drum*, Instrument*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<Piano*, Instrument*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<Whistle*, Instrument*>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<SnareDrum*, Drum*>));
}
ZTH_TEST("robust", "the Instrument destructor is virtual") {
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Instrument>);
}
ZTH_TEST("robust", "deleting a drum through an Instrument pointer destroys the whole drum (the drumhead is released)") {
    drumheads_alive = 0;
    {
        std::unique_ptr<Instrument> p = std::make_unique<Drum>();
        ZTH_EXPECT_EQ(drumheads_alive, 1);
    }
    ZTH_EXPECT_EQ(drumheads_alive, 0);
}
ZTH_TEST("robust", "a band is destroyed completely: no instrument and no drumhead is left") {
    instruments_alive = 0;
    drumheads_alive = 0;
    {
        Band band = make_band();
        band.push_back(std::make_unique<SnareDrum>());
        ZTH_EXPECT_EQ(instruments_alive, 5);
        ZTH_EXPECT_EQ(drumheads_alive, 2);
    }
    ZTH_EXPECT_EQ(instruments_alive, 0);
    ZTH_EXPECT_EQ(drumheads_alive, 0);
}
ZTH_TEST("robust", "copying an Instrument counts as one more, and destroying the copy as one less") {
    instruments_alive = 0;
    {
        Guitar g;
        ZTH_EXPECT_EQ(instruments_alive, 1);
        Guitar copy = g;
        ZTH_EXPECT_EQ(instruments_alive, 2);
    }
    ZTH_EXPECT_EQ(instruments_alive, 0);
}
ZTH_TEST("robust", "the library does not leak or double free") {
    zth::mem::Scope scope;
    {
        Band band = make_band();
        band.push_back(std::make_unique<SnareDrum>());
        rehearse(band, 3);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a Drum is played through ANY Instrument reference or pointer, even for a SnareDrum in a band") {
    Band band;
    band.push_back(std::make_unique<SnareDrum>());
    band.push_back(std::make_unique<Drum>("conga"));
    ZTH_EXPECT_EQ(band[0]->play(2), S("crack! crack!"));
    ZTH_EXPECT_EQ(band[1]->play(2), S("boom! boom!"));
    ZTH_EXPECT_EQ(band[1]->name(), S("conga"));
}
ZTH_TEST("robust", "hear reaches the override of a drum written by someone else: it takes a reference, not a copy") {
    Timpani t;
    ZTH_EXPECT_EQ(hear(t), S("rumble"));
    const Drum& as_drum = t;
    ZTH_EXPECT_EQ(hear(as_drum), S("rumble"));
}
ZTH_TEST("robust", "hear does not copy the drum") {
    drumheads_alive = 0;
    SnareDrum s;
    ZTH_EXPECT_EQ(drumheads_alive, 1);
    const int before = drumheads_alive;
    int seen_during = -1;
    struct Spy final : Drum {
        int* where;
        explicit Spy(int* w) : Drum("spy"), where(w) {}
        S sound() const override {
            *where = drumheads_alive;  // how many drums exist while hear() is calling sound()
            return "x";
        }
    } spy(&seen_during);
    ZTH_EXPECT_EQ(drumheads_alive, before + 1);
    hear(spy);
    ZTH_EXPECT_EQ(seen_during, before + 1);  // a by-value parameter would be a SECOND temporary drum (and a sliced one)
}
ZTH_TEST("robust", "copying a Drum counts one more drumhead, and destroying the copy releases it") {
    drumheads_alive = 0;
    {
        Drum d;
        Drum copy = d;
        ZTH_EXPECT_EQ(drumheads_alive, 2);
    }
    ZTH_EXPECT_EQ(drumheads_alive, 0);
}
