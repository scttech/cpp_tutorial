#pragma once
// READ-ONLY. The items, the player, and the counters that the Fast tests read.
#include <optional>
#include <string>
#include <variant>

namespace inv {

namespace stats {
inline unsigned long long copies = 0, moves = 0;  // items (and their strings) COPIED / MOVED
inline void reset() { copies = moves = 0; }
}  // namespace stats

struct Counter {
    Counter() = default;
    Counter(const Counter&) { ++stats::copies; }
    Counter(Counter&&) noexcept { ++stats::moves; }
    Counter& operator=(const Counter&) {
        ++stats::copies;
        return *this;
    }
    Counter& operator=(Counter&&) noexcept {
        ++stats::moves;
        return *this;
    }
};

inline constexpr int kMaxStack = 99;  // how many potions or gems fit in one slot

// A potion heals when used. Potions with the same name and heal amount stack (count of them in one slot).
struct Potion { std::string name; int heal = 0; int count = 1; [[no_unique_address]] Counter counter{}; };
// A weapon adds its damage to the player's attack while equipped. It never stacks.
struct Weapon { std::string name; int damage = 0; int durability = 100; [[no_unique_address]] Counter counter{}; };
// Armor adds its defense while equipped. It never stacks.
struct Armor { std::string name; int defense = 0; [[no_unique_address]] Counter counter{}; };
// A key opens a door elsewhere; it cannot be "used" from the inventory. It never stacks.
struct Key { std::string name; int door = 0; [[no_unique_address]] Counter counter{}; };
// A gem is worth gold. Gems with the same name and value stack. It cannot be used.
struct Gem { std::string name; int value = 0; int count = 1; [[no_unique_address]] Counter counter{}; };

using Item = std::variant<Potion, Weapon, Armor, Key, Gem>;

struct Player {
    int hp = 50;
    int max_hp = 100;
    int base_attack = 1;
    int base_defense = 0;
    std::optional<Weapon> weapon;  // equipped
    std::optional<Armor> armor;    // equipped
    int attack() const { return base_attack + (weapon ? weapon->damage : 0); }
    int defense() const { return base_defense + (armor ? armor->defense : 0); }
};

}  // namespace inv
