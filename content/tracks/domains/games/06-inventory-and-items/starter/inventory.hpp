#pragma once
// YOUR INVENTORY: a fixed number of slots, each empty or holding one Item (a std::variant of the item types in types.hpp).
//
//   Inventory(slots)
//   capacity(), used_slots()
//   add(item)      -> AddResult::Stacked  the whole item merged into existing stacks (no new slot used)
//                     AddResult::Added    at least one new slot (the lowest free one) was used
//                     AddResult::Full     it does not fit: NOTHING changes (all or nothing)
//                  Potions (same name + heal) and gems (same name + value) STACK, up to kMaxStack per slot: fill the existing
//                  stacks first (lowest slot first), then use new slots (a count of 150 becomes 99 + 51). Every other item
//                  takes a slot of its own. A stackable with count < 1 is invalid: Full.
//   at(slot)       the item, or nullptr for an empty or out-of-range slot
//   take(slot)     remove and return the item (std::nullopt if there is none)
//   use(slot, player) -> UseResult::NothingThere (empty / out of range)
//                     Potion: heals (never above max_hp) and uses one up (the slot empties with the last one).
//                             At full health: CannotUse and nothing is consumed.
//                     Weapon / Armor: equips it. If the player already had one equipped, THAT one takes the slot (a swap);
//                             otherwise the slot becomes empty. -> Used
//                     Key / Gem: CannotUse
//   find(name)     the lowest slot holding an item of that name
//   total_value()  gold value of all gems (value * count)
//
// Dispatch on the item type with std::visit or std::get_if; no inheritance, no virtual functions. Move items, don't copy them.
#include <cstddef>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "types.hpp"

namespace inv {

enum class AddResult { Stacked, Added, Full };
enum class UseResult { Used, NothingThere, CannotUse };

class Inventory {
public:
    explicit Inventory(std::size_t slots) : slots_(slots) {}

    std::size_t capacity() const { return slots_.size(); }
    std::size_t used_slots() const { return 0; }  // TODO

    AddResult add(Item item) {
        (void)item;
        return AddResult::Full;  // TODO
    }
    const Item* at(std::size_t slot) const {
        (void)slot;
        return nullptr;  // TODO
    }
    std::optional<Item> take(std::size_t slot) {
        (void)slot;
        return std::nullopt;  // TODO
    }
    UseResult use(std::size_t slot, Player& player) {
        (void)slot;
        (void)player;
        return UseResult::NothingThere;  // TODO
    }
    std::optional<std::size_t> find(std::string_view name) const {
        (void)name;
        return std::nullopt;  // TODO
    }
    long long total_value() const { return 0; }  // TODO

private:
    std::vector<std::optional<Item>> slots_;
};

}  // namespace inv
