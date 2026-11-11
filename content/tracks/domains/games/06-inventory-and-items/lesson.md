# Inventory and Item System

> **You're an engineer at a game studio.** The RPG's items are a class hierarchy: `Item` with `virtual use()`, and eleven subclasses. Adding "a new kind of effect" touches five files, every item is a heap allocation behind a pointer, and the save system needs a `dynamic_cast` ladder to know what it is looking at. The designers want the full list of item kinds to be **one type that the compiler checks**.

## The model

`types.hpp` defines five item structs and `using Item = std::variant<Potion, Weapon, Armor, Key, Gem>`: an `Item` *is* exactly one of them, stored inline (no pointer, no heap, no base class). `Player` holds equipped gear.

You write `Inventory` (in `inventory.hpp`): a fixed number of **slots**, each an `std::optional<Item>`.

- **`add`** is all-or-nothing. Potions (same name **and** heal) and gems (same name **and** value) **stack** up to `kMaxStack = 99` per slot: fill existing stacks first (lowest slot first), then take the lowest free slots, splitting a count of 150 into 99 + 51. If it does not all fit, return `Full` and change **nothing**. Everything else takes one slot. A stackable with `count < 1` is invalid.
- **`use`** behaves differently per type. That is what `std::visit` is for: one *visitor* with an overload per alternative, and the compiler refuses to compile if you forget a type.
  - *Potion*: heals up to `max_hp` and uses one up; at full health it is **not** consumed.
  - *Weapon / Armor*: equips; if something was already equipped, **that** goes back into the slot (a swap), otherwise the slot empties.
  - *Key / Gem*: `CannotUse`.
- `take`, `at`, `find`, `total_value`, `used_slots` as in the comments of the starter.

## The tiers

- **Works**: slots, stacking rules, the 99 limit and overflow, full inventory, take, healing, equipping and swapping, unusable items, find and total value.
- **Robust**: the exact-fit boundary and the no-change-on-failure rule, filling several partial stacks in order, invalid counts, a zero-slot inventory, clamped healing and potions above max hp, equipping with a full bag, same names across types, and a random test that potions are neither lost nor invented.
- **Fast**: adding unstackable items moves them (at most twice), no copies; equipping swaps with moves only (20000 swaps, no copy); lookups are single passes.
- **Clean**: no inheritance, no virtual functions, no `dynamic_cast`/`typeid`/`std::any`, `std::visit` or `std::get_if` for dispatch.

## Tips

Two small helpers make the stacking code short: `count_of(Item&)` returning a pointer to the count of a potion or gem (or `nullptr`), and `same_kind(a, b)`. For `use`, an `Overloaded` struct that inherits from several lambdas (`using Fs::operator()...`) is the standard way to write a variant visitor.
