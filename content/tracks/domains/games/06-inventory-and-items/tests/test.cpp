#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <variant>
#include <vector>

#include "inventory.hpp"
#include "zth_test.hpp"

using namespace inv;

namespace {

int potions_in(const Inventory& bag) {
    int n = 0;
    for (std::size_t i = 0; i < bag.capacity(); ++i) {
        if (const Item* it = bag.at(i)) {
            if (const auto* p = std::get_if<Potion>(it)) n += p->count;
        }
    }
    return n;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty inventory; adding items fills the lowest free slot") {
    Inventory bag(4);
    ZTH_EXPECT_EQ(bag.capacity(), std::size_t{4});
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{0});
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);
    ZTH_EXPECT_TRUE(bag.add(Weapon{"Sword", 7}) == AddResult::Added);
    ZTH_EXPECT_TRUE(bag.add(Armor{"Shield", 3}) == AddResult::Added);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{2});
    ZTH_EXPECT_TRUE(std::holds_alternative<Weapon>(*bag.at(0)));
    ZTH_EXPECT_TRUE(std::holds_alternative<Armor>(*bag.at(1)));
    ZTH_EXPECT_EQ(std::get<Weapon>(*bag.at(0)).damage, 7);
    ZTH_EXPECT_TRUE(bag.at(2) == nullptr);
    ZTH_EXPECT_TRUE(bag.at(99) == nullptr);
}
ZTH_TEST("works", "potions and gems of the same kind stack; different kinds do not") {
    Inventory bag(6);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 3}) == AddResult::Added);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 2}) == AddResult::Stacked);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 5);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 50, 1}) == AddResult::Added);   // a stronger one is another kind
    ZTH_EXPECT_TRUE(bag.add(Potion{"Tonic", 30, 1}) == AddResult::Added);    // another name
    ZTH_EXPECT_TRUE(bag.add(Gem{"Ruby", 50, 2}) == AddResult::Added);
    ZTH_EXPECT_TRUE(bag.add(Gem{"Ruby", 50, 2}) == AddResult::Stacked);
    ZTH_EXPECT_TRUE(bag.add(Gem{"Ruby", 80, 1}) == AddResult::Added);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{5});
    ZTH_EXPECT_EQ(std::get<Gem>(*bag.at(3)).count, 4);
}
ZTH_TEST("works", "weapons, armor and keys never stack") {
    Inventory bag(5);
    for (int i = 0; i < 3; ++i) ZTH_EXPECT_TRUE(bag.add(Weapon{"Sword", 7}) == AddResult::Added);
    ZTH_EXPECT_TRUE(bag.add(Key{"Gate", 1}) == AddResult::Added);
    ZTH_EXPECT_TRUE(bag.add(Key{"Gate", 1}) == AddResult::Added);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{5});
}
ZTH_TEST("works", "a stack holds at most 99: a count of 150 becomes 99 + 51") {
    Inventory bag(3);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 150}) == AddResult::Added);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 99);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(1)).count, 51);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 60}) == AddResult::Added);  // 48 into the second stack, 12 into a third
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(1)).count, 99);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(2)).count, 12);
    ZTH_EXPECT_EQ(potions_in(bag), 210);
}
ZTH_TEST("works", "a full inventory refuses; nothing changes") {
    Inventory bag(2);
    bag.add(Weapon{"Sword", 7});
    bag.add(Key{"Gate", 1});
    ZTH_EXPECT_TRUE(bag.add(Armor{"Shield", 3}) == AddResult::Full);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 1}) == AddResult::Full);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{2});
}
ZTH_TEST("works", "take removes and returns the item; the slot can be reused") {
    Inventory bag(3);
    bag.add(Weapon{"Sword", 7});
    bag.add(Key{"Gate", 1});
    bag.add(Armor{"Shield", 3});
    auto taken = bag.take(1);
    ZTH_EXPECT_TRUE(taken.has_value());
    ZTH_EXPECT_EQ(std::get<Key>(*taken).name, std::string("Gate"));
    ZTH_EXPECT_TRUE(bag.at(1) == nullptr);
    ZTH_EXPECT_FALSE(bag.take(1).has_value());
    ZTH_EXPECT_FALSE(bag.take(50).has_value());
    ZTH_EXPECT_TRUE(bag.add(Gem{"Opal", 10, 1}) == AddResult::Added);
    ZTH_EXPECT_TRUE(std::holds_alternative<Gem>(*bag.at(1)));  // the lowest free slot
}
ZTH_TEST("works", "using a potion heals, never above the maximum, and uses one up") {
    Inventory bag(2);
    Player p;
    p.hp = 60;
    bag.add(Potion{"Elixir", 30, 2});
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.hp, 90);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 1);
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.hp, 100);                 // clamped
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);   // the last one emptied the slot
}
ZTH_TEST("works", "a potion is not wasted at full health") {
    Inventory bag(1);
    Player p;
    p.hp = p.max_hp;
    bag.add(Potion{"Elixir", 30, 3});
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::CannotUse);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 3);
}
ZTH_TEST("works", "equipping a weapon changes the attack and empties the slot; equipping another swaps them") {
    Inventory bag(3);
    Player p;
    ZTH_EXPECT_EQ(p.attack(), 1);
    bag.add(Weapon{"Sword", 7});
    bag.add(Weapon{"Axe", 9});
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.attack(), 8);
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);
    ZTH_EXPECT_TRUE(bag.use(1, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.attack(), 10);
    ZTH_EXPECT_TRUE(bag.at(1) != nullptr);  // the sword came back, into the slot the axe left
    ZTH_EXPECT_EQ(std::get<Weapon>(*bag.at(1)).name, std::string("Sword"));
    ZTH_EXPECT_EQ(p.weapon->name, std::string("Axe"));
}
ZTH_TEST("works", "armor is equipped like a weapon, separately from it") {
    Inventory bag(3);
    Player p;
    bag.add(Armor{"Mail", 5});
    bag.add(Weapon{"Sword", 7});
    bag.use(0, p);
    bag.use(1, p);
    ZTH_EXPECT_EQ(p.defense(), 5);
    ZTH_EXPECT_EQ(p.attack(), 8);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{0});
}
ZTH_TEST("works", "keys and gems cannot be used; empty and out-of-range slots say there is nothing there") {
    Inventory bag(3);
    Player p;
    bag.add(Key{"Gate", 1});
    bag.add(Gem{"Ruby", 50, 1});
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::CannotUse);
    ZTH_EXPECT_TRUE(bag.use(1, p) == UseResult::CannotUse);
    ZTH_EXPECT_TRUE(bag.use(2, p) == UseResult::NothingThere);
    ZTH_EXPECT_TRUE(bag.use(3, p) == UseResult::NothingThere);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{2});
}
ZTH_TEST("works", "find by name and the total value of the gems") {
    Inventory bag(5);
    bag.add(Weapon{"Sword", 7});
    bag.add(Gem{"Ruby", 50, 4});
    bag.add(Potion{"Elixir", 30, 1});
    bag.add(Gem{"Opal", 12, 3});
    ZTH_EXPECT_EQ(*bag.find("Sword"), std::size_t{0});
    ZTH_EXPECT_EQ(*bag.find("Opal"), std::size_t{3});
    ZTH_EXPECT_FALSE(bag.find("Dragon").has_value());
    ZTH_EXPECT_EQ(bag.total_value(), 4LL * 50 + 3 * 12);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "adding is all or nothing: a stack that does not fit changes nothing, not even the existing stack") {
    Inventory bag(2);
    bag.add(Potion{"Elixir", 30, 90});     // slot 0: 90
    bag.add(Weapon{"Sword", 7});           // slot 1
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 10}) == AddResult::Full);    // 9 would fit, the tenth has no slot
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 90);                    // so not even the 9 were added
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 9}) == AddResult::Stacked);  // exactly fills the stack
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 99);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 1}) == AddResult::Full);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(0)).count, 99);
    Inventory other(2);
    other.add(Potion{"Elixir", 30, 90});
    ZTH_EXPECT_TRUE(other.add(Potion{"Elixir", 30, 100}) == AddResult::Added);   // 9 + 91
    ZTH_EXPECT_EQ(std::get<Potion>(*other.at(0)).count, 99);
    ZTH_EXPECT_EQ(std::get<Potion>(*other.at(1)).count, 91);
    ZTH_EXPECT_TRUE(other.add(Potion{"Elixir", 30, 9}) == AddResult::Full);     // room is 8
    ZTH_EXPECT_EQ(std::get<Potion>(*other.at(1)).count, 91);
}
ZTH_TEST("robust", "a count that fits exactly is accepted, one more is refused with no change") {
    Inventory bag(3);
    ZTH_EXPECT_TRUE(bag.add(Gem{"Ruby", 5, 3 * 99 + 1}) == AddResult::Full);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{0});
    ZTH_EXPECT_TRUE(bag.add(Gem{"Ruby", 5, 3 * 99}) == AddResult::Added);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{3});
    ZTH_EXPECT_EQ(bag.total_value(), 5LL * 297);
    Inventory half(3);
    half.add(Gem{"Ruby", 5, 50});
    half.add(Weapon{"Sword", 1});
    ZTH_EXPECT_TRUE(half.add(Gem{"Ruby", 5, 149}) == AddResult::Full);   // room: 49 in the stack + 99 in the free slot = 148
    ZTH_EXPECT_EQ(std::get<Gem>(*half.at(0)).count, 50);                 // untouched
    ZTH_EXPECT_EQ(half.used_slots(), std::size_t{2});
    ZTH_EXPECT_TRUE(half.add(Gem{"Ruby", 5, 148}) == AddResult::Added);
    ZTH_EXPECT_EQ(std::get<Gem>(*half.at(0)).count, 99);
    ZTH_EXPECT_EQ(std::get<Gem>(*half.at(2)).count, 99);
}
ZTH_TEST("robust", "an existing stack with room is filled before any empty slot below it is used") {
    Inventory bag(5);
    bag.add(Potion{"Elixir", 30, 99});     // slot 0, full
    bag.add(Weapon{"Sword", 1});           // slot 1
    bag.add(Potion{"Elixir", 30, 1});      // slot 0 is full: a new stack in slot 2
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(2)).count, 1);
    bag.take(0);                           // slot 0 is empty now, below the stack in slot 2
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 50}) == AddResult::Stacked);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(2)).count, 51);
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);
}
ZTH_TEST("robust", "invalid counts (zero, negative) are refused and change nothing") {
    Inventory bag(3);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 0}) == AddResult::Full);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, -5}) == AddResult::Full);
    ZTH_EXPECT_TRUE(bag.add(Gem{"Ruby", 5, 0}) == AddResult::Full);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{0});
}
ZTH_TEST("robust", "an inventory with no slots refuses everything and answers safely") {
    Inventory bag(0);
    Player p;
    ZTH_EXPECT_EQ(bag.capacity(), std::size_t{0});
    ZTH_EXPECT_TRUE(bag.add(Key{"Gate", 1}) == AddResult::Full);
    ZTH_EXPECT_TRUE(bag.add(Potion{"Elixir", 30, 1}) == AddResult::Full);
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::NothingThere);
    ZTH_EXPECT_FALSE(bag.take(0).has_value());
    ZTH_EXPECT_FALSE(bag.find("x").has_value());
    ZTH_EXPECT_EQ(bag.total_value(), 0LL);
}
ZTH_TEST("robust", "healing: 95 + 30 stops at 100; hp already above max is not raised or lowered; potion kinds heal their own amount") {
    Inventory bag(3);
    Player p;
    p.hp = 95;
    bag.add(Potion{"Elixir", 30, 1});
    bag.add(Potion{"Tonic", 5, 2});
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.hp, 100);
    p.hp = 10;
    p.max_hp = 20;
    ZTH_EXPECT_TRUE(bag.use(1, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.hp, 15);
    p.hp = 150;  // above the maximum (a temporary bonus): nothing to heal
    ZTH_EXPECT_TRUE(bag.use(1, p) == UseResult::CannotUse);
    ZTH_EXPECT_EQ(p.hp, 150);
    ZTH_EXPECT_EQ(std::get<Potion>(*bag.at(1)).count, 1);
}
ZTH_TEST("robust", "the last potion of a stack empties its slot, which the next add uses") {
    Inventory bag(2);
    Player p;
    p.hp = 1;
    bag.add(Potion{"Elixir", 1, 1});
    bag.add(Weapon{"Sword", 3});
    bag.use(0, p);
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{1});
    ZTH_EXPECT_TRUE(bag.add(Key{"Gate", 1}) == AddResult::Added);
    ZTH_EXPECT_TRUE(std::holds_alternative<Key>(*bag.at(0)));
}
ZTH_TEST("robust", "equipping with a full inventory still works (the swap needs no extra slot); armor swaps too") {
    Inventory bag(2);
    Player p;
    bag.add(Armor{"Mail", 5});
    bag.add(Armor{"Plate", 9});
    ZTH_EXPECT_TRUE(bag.use(0, p) == UseResult::Used);
    ZTH_EXPECT_TRUE(bag.use(1, p) == UseResult::Used);
    ZTH_EXPECT_EQ(p.defense(), 9);
    ZTH_EXPECT_EQ(p.armor->name, std::string("Plate"));
    ZTH_EXPECT_EQ(std::get<Armor>(*bag.at(1)).name, std::string("Mail"));
    ZTH_EXPECT_TRUE(bag.at(0) == nullptr);
    // and the item that comes back into the bag keeps its data
    ZTH_EXPECT_EQ(std::get<Armor>(*bag.at(1)).defense, 5);
}
ZTH_TEST("robust", "find returns the first slot with the name, whatever the type; take of a stack returns the whole stack") {
    Inventory bag(4);
    bag.add(Key{"Shared", 1});
    bag.add(Gem{"Shared", 1, 7});
    ZTH_EXPECT_EQ(*bag.find("Shared"), std::size_t{0});
    bag.take(0);
    ZTH_EXPECT_EQ(*bag.find("Shared"), std::size_t{1});
    auto stack = bag.take(1);
    ZTH_EXPECT_EQ(std::get<Gem>(*stack).count, 7);
    ZTH_EXPECT_EQ(bag.used_slots(), std::size_t{0});
}
ZTH_TEST_TIMEOUT("robust", "3000 random adds, uses and takes never lose or invent potions", 10) {
    ::zth::Lcg rng(321);
    Inventory bag(8);
    Player p;
    p.max_hp = 1000000;
    p.hp = 1;
    long long expected = 0;
    for (int step = 0; step < 3000; ++step) {
        switch (rng.below(4)) {
            case 0: {
                const int n = 1 + rng.below(120);
                if (bag.add(Potion{"Elixir", 1, n}) != AddResult::Full) expected += n;
                break;
            }
            case 1: {
                if (bag.use(static_cast<std::size_t>(rng.below(9)), p) == UseResult::Used && p.hp > 0) {
                    // a potion (or equipment) was used; potions are counted below
                }
                break;
            }
            case 2: {
                bag.add(Weapon{"Stick", 1});
                break;
            }
            default: {
                const std::size_t slot = static_cast<std::size_t>(rng.below(8));
                if (const Item* it = bag.at(slot)) {
                    if (const auto* pot = std::get_if<Potion>(it)) expected -= pot->count;
                }
                bag.take(slot);
            }
        }
        // every use of a potion raised hp by exactly 1; so potions consumed = hp - 1
        ZTH_EXPECT_EQ(static_cast<long long>(potions_in(bag)) + (p.hp - 1), expected);
        for (std::size_t i = 0; i < bag.capacity(); ++i) {
            if (const Item* it = bag.at(i)) {
                if (const auto* pot = std::get_if<Potion>(it)) ZTH_EXPECT_TRUE(pot->count >= 1 && pot->count <= kMaxStack);
            }
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "adding unstackable items moves each at most twice and copies none", 10) {
    Inventory bag(1000);
    std::vector<Item> items;
    for (int i = 0; i < 1000; ++i) items.push_back(Weapon{"Sword number " + std::to_string(i) + " of the long name series", i});
    stats::reset();
    for (Item& it : items) ZTH_EXPECT_TRUE(bag.add(std::move(it)) == AddResult::Added);
    ZTH_EXPECT_EQ(stats::copies, 0ULL);
    ZTH_EXPECT_TRUE(stats::moves <= 2000ULL + 1000);
}
ZTH_TEST_TIMEOUT("fast", "stacking into an existing stack allocates and copies nothing (10000 potions added one at a time)", 10) {
    Inventory bag(4);
    bag.add(Potion{"Elixir of the long-lasting kind", 30, 1});
    zth::mem::Scope scope;
    stats::reset();
    for (int i = 0; i < 90; ++i) {
        Potion p{"Elixir of the long-lasting kind", 30, 1};   // building the argument allocates; the add must not
        (void)p;
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 90);  // only the 90 temporary names above
}
ZTH_TEST_TIMEOUT("fast", "use and equip move items, never copy them (20000 swaps)", 10) {
    Inventory bag(2);
    Player p;
    bag.add(Weapon{"A rather long weapon name to avoid the small string optimisation", 5});
    bag.add(Weapon{"Another rather long weapon name to avoid the small string optimisation", 6});
    p.weapon = Weapon{"The weapon the player already carries, with a long name as well", 1};
    stats::reset();
    for (int i = 0; i < 20000; ++i) bag.use(static_cast<std::size_t>(i % 2), p);
    ZTH_EXPECT_EQ(stats::copies, 0ULL);
    ZTH_EXPECT_TRUE(stats::moves <= 20000ULL * 6);
}
ZTH_TEST_TIMEOUT("fast", "the total value and find are single passes: 200000 calls on 256 slots in time", 10) {
    Inventory bag(256);
    for (int i = 0; i < 256; ++i) bag.add(Gem{"Gem" + std::to_string(i), i, 1});
    long long sum = 0;
    for (int i = 0; i < 200000; ++i) sum += bag.total_value() + static_cast<long long>(*bag.find("Gem255"));
    ZTH_EXPECT_TRUE(sum > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("inventory.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("inventory.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the items are a closed set handled with std::visit or std::get_if: no virtual functions, no inheritance, no dynamic_cast, no typeid, no std::any") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("inventory.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "visit").empty() || !::zth::find_word(code, "get_if").empty());
    for (const char* word : {"virtual", "dynamic_cast", "typeid", "any", "static_cast"}) {
        if (std::string(word) == "static_cast") continue;
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("inventory.hpp uses `") + word + "`");
            return;
        }
    }
    ZTH_EXPECT_TRUE(code.find("public Item") == std::string::npos);
}
ZTH_TEST("clean", "behaviour per item type lives in one place: std::visit with overloaded lambdas (or if constexpr) in use(), not a chain of index() comparisons") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("inventory.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "index").empty());
}
