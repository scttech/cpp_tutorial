// A backpack with 6 slots: potions stack, a sword is equipped, gems are counted. Read-only. Press Run.
#include <cstdio>
#include <string>
#include <variant>

#include "inventory.hpp"

static std::string describe(const inv::Item* item) {
    if (!item) return "(empty)";
    struct Show {
        std::string operator()(const inv::Potion& p) const { return "potion " + p.name + " x" + std::to_string(p.count); }
        std::string operator()(const inv::Weapon& w) const { return "weapon " + w.name + " (+" + std::to_string(w.damage) + ")"; }
        std::string operator()(const inv::Armor& a) const { return "armor " + a.name + " (+" + std::to_string(a.defense) + ")"; }
        std::string operator()(const inv::Key& k) const { return "key " + k.name; }
        std::string operator()(const inv::Gem& g) const { return "gem " + g.name + " x" + std::to_string(g.count); }
    };
    return std::visit(Show{}, *item);
}

int main() {
    inv::Inventory bag(6);
    inv::Player hero;
    bag.add(inv::Potion{"Elixir", 30, 3});
    bag.add(inv::Weapon{"Sword", 7});
    bag.add(inv::Potion{"Elixir", 30, 2});   // stacks with the first
    bag.add(inv::Gem{"Ruby", 50, 4});
    bag.add(inv::Key{"Cellar key", 12});
    bag.add(inv::Weapon{"Axe", 9});
    for (std::size_t i = 0; i < bag.capacity(); ++i) std::printf("slot %zu: %s\n", i, describe(bag.at(i)).c_str());
    std::printf("hp %d, attack %d\n", hero.hp, hero.attack());

    bag.use(0, hero);  // drink an elixir
    bag.use(1, hero);  // equip the sword
    std::printf("after: hp %d, attack %d, elixirs %s\n", hero.hp, hero.attack(), describe(bag.at(0)).c_str());
    bag.use(5, hero);  // equip the axe: the sword goes back into slot 5
    std::printf("axe equipped: attack %d, slot 5 now: %s\n", hero.attack(), describe(bag.at(5)).c_str());
    std::printf("gems are worth %lld gold\n", bag.total_value());
}
