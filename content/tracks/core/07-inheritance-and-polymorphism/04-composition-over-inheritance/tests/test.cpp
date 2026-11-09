#define ZTH_TRACK_ALLOCATIONS
#include <climits>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "composition.hpp"
#include "zth_test.hpp"

namespace {

using Ints = std::vector<int>;

// Parts written by the TEST: a character built from any Movement and Attack works, nothing in Character needs to know them.
struct Swim : Movement {
    static inline int alive = 0;
    Swim() { ++alive; }
    ~Swim() override { --alive; }
    int speed() const override { return 2; }
    std::string name() const override { return "swim"; }
};
struct Magic : Attack {
    int damage(int level) const override { return 100 * level; }
    std::string name() const override { return "magic"; }
};

template <class F>
bool throws_invalid(F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

Character orc(int hp = 10) { return Character("Orc", hp, std::make_unique<Walk>(), std::make_unique<Sword>()); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "CountingBag: add counts one value") {
    CountingBag b;
    b.add(5);
    b.add(7);
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    ZTH_EXPECT_EQ(b.sum(), 12LL);
    ZTH_EXPECT_EQ(b.added(), std::size_t{2});
}
ZTH_TEST("works", "CountingBag: add_all counts each value ONCE (not twice, like the inheriting version)") {
    CountingBag b;
    b.add_all({1, 2, 3});
    ZTH_EXPECT_EQ(b.size(), std::size_t{3});
    ZTH_EXPECT_EQ(b.sum(), 6LL);
    ZTH_EXPECT_EQ(b.added(), std::size_t{3});
}
ZTH_TEST("works", "CountingBag: add and add_all together") {
    CountingBag b;
    b.add(10);
    b.add_all({1, 2});
    b.add(20);
    b.add_all({3});
    ZTH_EXPECT_EQ(b.added(), std::size_t{5});
    ZTH_EXPECT_EQ(b.size(), std::size_t{5});
    ZTH_EXPECT_EQ(b.sum(), 36LL);
}
ZTH_TEST("works", "Walk, Fly, Sword and Bow") {
    ZTH_EXPECT_EQ(Walk().speed(), 1);
    ZTH_EXPECT_EQ(Walk().name(), std::string("walk"));
    ZTH_EXPECT_EQ(Fly().speed(), 3);
    ZTH_EXPECT_EQ(Fly().name(), std::string("fly"));
    ZTH_EXPECT_EQ(Sword().damage(0), 5);
    ZTH_EXPECT_EQ(Sword().damage(3), 11);
    ZTH_EXPECT_EQ(Sword().name(), std::string("sword"));
    ZTH_EXPECT_EQ(Bow().damage(0), 3);
    ZTH_EXPECT_EQ(Bow().damage(4), 7);
    ZTH_EXPECT_EQ(Bow().name(), std::string("bow"));
}
ZTH_TEST("works", "Character: name, hp, alive, describe") {
    Character c = orc(10);
    ZTH_EXPECT_EQ(c.name(), std::string("Orc"));
    ZTH_EXPECT_EQ(c.hp(), 10);
    ZTH_EXPECT_TRUE(c.alive());
    ZTH_EXPECT_EQ(c.describe(), std::string("Orc (10 hp, walk / sword)"));
}
ZTH_TEST("works", "Character: take_hit lowers the hp") {
    Character c = orc(10);
    c.take_hit(4);
    ZTH_EXPECT_EQ(c.hp(), 6);
    ZTH_EXPECT_EQ(c.describe(), std::string("Orc (6 hp, walk / sword)"));
}
ZTH_TEST("works", "Character: distance and strike come from the parts") {
    Character c = orc();
    ZTH_EXPECT_EQ(c.distance(5), 5);
    ZTH_EXPECT_EQ(c.strike(2), 9);
    Character archer("Robin", 20, std::make_unique<Walk>(), std::make_unique<Bow>());
    ZTH_EXPECT_EQ(archer.strike(2), 5);
    ZTH_EXPECT_EQ(archer.describe(), std::string("Robin (20 hp, walk / bow)"));
}
ZTH_TEST("works", "set_movement swaps the movement while the program runs") {
    Character c = orc();
    ZTH_EXPECT_EQ(c.distance(4), 4);
    c.set_movement(std::make_unique<Fly>());
    ZTH_EXPECT_EQ(c.distance(4), 12);
    ZTH_EXPECT_EQ(c.describe(), std::string("Orc (10 hp, fly / sword)"));
}
ZTH_TEST("works", "make_knight and make_dragon") {
    Character k = make_knight("Arthur");
    ZTH_EXPECT_EQ(k.describe(), std::string("Arthur (30 hp, walk / sword)"));
    Character d = make_dragon("Smaug");
    ZTH_EXPECT_EQ(d.describe(), std::string("Smaug (100 hp, fly / sword)"));
    ZTH_EXPECT_EQ(d.distance(2), 6);
}
ZTH_TEST("works", "a part written by someone else plugs in (open for extension)") {
    Character c("Eel", 5, std::make_unique<Swim>(), std::make_unique<Magic>());
    ZTH_EXPECT_EQ(c.distance(5), 10);
    ZTH_EXPECT_EQ(c.strike(2), 200);
    ZTH_EXPECT_EQ(c.describe(), std::string("Eel (5 hp, swim / magic)"));
}
ZTH_TEST("works", "the diamond: Copier works and builds ONE Device") {
    Device::constructed = 0;
    Copier c(7);
    ZTH_EXPECT_EQ(c.copy(), std::string("scan:7+print:7"));
    ZTH_EXPECT_EQ(Device::constructed, 1);
}
ZTH_TEST("works", "the diamond: a Scanner and a Printer on their own") {
    Scanner s(3);
    Printer p(4);
    ZTH_EXPECT_EQ(s.scan(), std::string("scan:3"));
    ZTH_EXPECT_EQ(p.print(), std::string("print:4"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "CountingBag is NOT an IntBag: composition, not inheritance") {
    ZTH_EXPECT_FALSE((std::is_base_of_v<IntBag, CountingBag>));
    ZTH_EXPECT_FALSE((std::is_convertible_v<CountingBag*, IntBag*>));
    ZTH_EXPECT_FALSE(std::is_polymorphic_v<CountingBag>);
}
ZTH_TEST("robust", "CountingBag: empty add_all, repeated calls, and large sums") {
    CountingBag b;
    b.add_all({});
    ZTH_EXPECT_EQ(b.added(), std::size_t{0});
    b.add_all({INT_MAX, INT_MAX});
    ZTH_EXPECT_EQ(b.sum(), 2LL * INT_MAX);
    b.add_all(Ints(1000, 1));
    ZTH_EXPECT_EQ(b.added(), std::size_t{1002});
    ZTH_EXPECT_EQ(b.size(), b.added());
}
ZTH_TEST("robust", "a Character HAS parts, it is not built by inheritance: no base class, not polymorphic, not copyable") {
    ZTH_EXPECT_FALSE(std::is_polymorphic_v<Character>);
    ZTH_EXPECT_FALSE(std::is_copy_constructible_v<Character>);
    ZTH_EXPECT_TRUE(std::is_move_constructible_v<Character>);
}
ZTH_TEST("robust", "Movement and Attack are interfaces: abstract with a virtual destructor; the concrete parts are final") {
    ZTH_EXPECT_TRUE(std::is_abstract_v<Movement>);
    ZTH_EXPECT_TRUE(std::is_abstract_v<Attack>);
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Movement>);
    ZTH_EXPECT_TRUE(std::has_virtual_destructor_v<Attack>);
    ZTH_EXPECT_TRUE(std::is_final_v<Walk> && std::is_final_v<Fly> && std::is_final_v<Sword> && std::is_final_v<Bow>);
}
ZTH_TEST("robust", "an invalid hp or a missing part is std::invalid_argument") {
    ZTH_EXPECT_TRUE(throws_invalid([] { orc(0); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { orc(-5); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Character c("X", 5, nullptr, std::make_unique<Sword>()); }));
    ZTH_EXPECT_TRUE(throws_invalid([] { Character c("X", 5, std::make_unique<Walk>(), nullptr); }));
    ZTH_EXPECT_EQ(orc(1).hp(), 1);
}
ZTH_TEST("robust", "take_hit: exactly the hp kills, overkill stops at 0, a negative hit is ignored") {
    Character c = orc(10);
    c.take_hit(-5);
    ZTH_EXPECT_EQ(c.hp(), 10);
    c.take_hit(0);
    ZTH_EXPECT_EQ(c.hp(), 10);
    c.take_hit(9);
    ZTH_EXPECT_EQ(c.hp(), 1);
    ZTH_EXPECT_TRUE(c.alive());
    c.take_hit(1);
    ZTH_EXPECT_EQ(c.hp(), 0);
    ZTH_EXPECT_FALSE(c.alive());
    Character d = orc(10);
    d.take_hit(1000);
    ZTH_EXPECT_EQ(d.hp(), 0);
    ZTH_EXPECT_FALSE(d.alive());
}
ZTH_TEST("robust", "the dead do not strike; the living do, even with 1 hp") {
    Character c = orc(2);
    c.take_hit(1);
    ZTH_EXPECT_EQ(c.strike(1), 7);
    c.take_hit(1);
    ZTH_EXPECT_EQ(c.strike(1), 0);
}
ZTH_TEST("robust", "distance of zero or negative ticks is 0") {
    Character c = make_dragon("D");
    ZTH_EXPECT_EQ(c.distance(0), 0);
    ZTH_EXPECT_EQ(c.distance(-3), 0);
    ZTH_EXPECT_EQ(c.distance(1), 3);
}
ZTH_TEST("robust", "set_movement(nullptr) throws and keeps the old movement") {
    Character c = orc();
    ZTH_EXPECT_TRUE(throws_invalid([&] { c.set_movement(nullptr); }));
    ZTH_EXPECT_EQ(c.distance(3), 3);
    ZTH_EXPECT_EQ(c.describe(), std::string("Orc (10 hp, walk / sword)"));
}
ZTH_TEST("robust", "swapping the movement leaves the attack and the hp alone, and characters are independent") {
    Character a = orc(10), b = orc(10);
    a.set_movement(std::make_unique<Fly>());
    ZTH_EXPECT_EQ(a.strike(1), 7);
    ZTH_EXPECT_EQ(a.hp(), 10);
    ZTH_EXPECT_EQ(b.distance(2), 2);
}
ZTH_TEST("robust", "the parts are destroyed with the character, and the old movement when it is swapped out (virtual destructor)") {
    Swim::alive = 0;
    {
        Character c("Eel", 5, std::make_unique<Swim>(), std::make_unique<Sword>());
        ZTH_EXPECT_EQ(Swim::alive, 1);
        c.set_movement(std::make_unique<Walk>());
        ZTH_EXPECT_EQ(Swim::alive, 0);  // the old part was released at once
        c.set_movement(std::make_unique<Swim>());
        ZTH_EXPECT_EQ(Swim::alive, 1);
    }
    ZTH_EXPECT_EQ(Swim::alive, 0);
}
ZTH_TEST("robust", "a Character can be moved (it owns its parts) without leaks or double frees") {
    zth::mem::Scope scope;
    {
        Character a = make_knight("A");
        Character b = std::move(a);
        std::vector<Character> v;
        v.push_back(std::move(b));
        v.push_back(make_dragon("D"));
        ZTH_EXPECT_EQ(v[0].describe(), std::string("A (30 hp, walk / sword)"));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "diamond: the Scanner view and the Printer view of a Copier share ONE id") {
    Copier c(7);
    Scanner& s = c;
    Printer& p = c;
    ZTH_EXPECT_EQ(s.id, 7);
    ZTH_EXPECT_EQ(p.id, 7);
    s.id = 9;
    ZTH_EXPECT_EQ(p.id, 9);
    ZTH_EXPECT_EQ(c.copy(), std::string("scan:9+print:9"));
}
ZTH_TEST("robust", "diamond: each Copier builds exactly one Device, also with several copiers") {
    Device::constructed = 0;
    Copier a(1), b(2);
    ZTH_EXPECT_EQ(Device::constructed, 2);
    ZTH_EXPECT_EQ(a.copy(), std::string("scan:1+print:1"));
    ZTH_EXPECT_EQ(b.copy(), std::string("scan:2+print:2"));
}
ZTH_TEST("robust", "diamond: Scanner and Printer derive from Device through a VIRTUAL base (a Copier is smaller than two Devices plus the rest)") {
    // with ordinary inheritance a Copier holds two Device objects (two ints); with a virtual base, one int plus bookkeeping
    Copier c(1);
    ZTH_EXPECT_TRUE((std::is_base_of_v<Device, Copier>));
    ZTH_EXPECT_TRUE((std::is_convertible_v<Copier*, Device*>));  // unambiguous only when the Device base is shared
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "composition.hpp does not use new, delete, malloc or free: unique_ptr owns the parts") {
    ZTH_EXPECT_NO_RAW_MEMORY("composition.hpp");
}
ZTH_TEST("clean", "composition.hpp has no `protected` members: composition has no use for them") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("composition.hpp"));
    ZTH_EXPECT_EQ(::zth::find_word(code, "protected").size(), std::size_t{0});
}
ZTH_TEST("clean", "every overriding function is marked override: at least 8 times") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("composition.hpp"));
    const std::size_t n = ::zth::find_word(code, "override").size();
    if (n < 8) {
        ::zth::fail("composition.hpp uses `override` " + std::to_string(n) + " time(s): Walk, Fly, Sword and Bow override 2 functions each (8)");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "composition.hpp has no `using namespace`") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("composition.hpp").using_directive);
}
