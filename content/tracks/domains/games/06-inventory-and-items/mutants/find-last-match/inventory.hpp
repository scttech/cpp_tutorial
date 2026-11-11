// MUTANT: find returns the highest slot
#pragma once
// A fixed array of optional<Item>. Behaviour is chosen by the type held in the variant: std::visit with overloaded lambdas.
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "types.hpp"

namespace inv {

enum class AddResult { Stacked, Added, Full };
enum class UseResult { Used, NothingThere, CannotUse };

namespace detail {

template <class... Fs>
struct Overloaded : Fs... {
    using Fs::operator()...;
};
template <class... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

// the count of a stackable item, nullptr for the others
inline int* count_of(Item& item) {
    if (auto* p = std::get_if<Potion>(&item)) return &p->count;
    if (auto* g = std::get_if<Gem>(&item)) return &g->count;
    return nullptr;
}
inline const int* count_of(const Item& item) {
    if (const auto* p = std::get_if<Potion>(&item)) return &p->count;
    if (const auto* g = std::get_if<Gem>(&item)) return &g->count;
    return nullptr;
}

// can these two items share a slot?
inline bool same_kind(const Item& a, const Item& b) {
    if (const auto* x = std::get_if<Potion>(&a)) {
        const auto* y = std::get_if<Potion>(&b);
        return y && x->name == y->name && x->heal == y->heal;
    }
    if (const auto* x = std::get_if<Gem>(&a)) {
        const auto* y = std::get_if<Gem>(&b);
        return y && x->name == y->name && x->value == y->value;
    }
    return false;
}

inline std::string_view name_of(const Item& item) {
    return std::visit([](const auto& x) -> std::string_view { return x.name; }, item);
}

}  // namespace detail

class Inventory {
public:
    explicit Inventory(std::size_t slots) : slots_(slots) {}

    std::size_t capacity() const { return slots_.size(); }
    std::size_t used_slots() const {
        return static_cast<std::size_t>(std::count_if(slots_.begin(), slots_.end(), [](const std::optional<Item>& s) { return s.has_value(); }));
    }

    AddResult add(Item item) {
        int* count = detail::count_of(item);
        if (count == nullptr) {  // one slot of its own
            const auto free_slot = first_free();
            if (!free_slot) return AddResult::Full;
            slots_[*free_slot] = std::move(item);
            return AddResult::Added;
        }
        if (*count < 1) return AddResult::Full;

        // all or nothing: first see whether it fits
        long long room = 0, free_slots = 0;
        for (const std::optional<Item>& s : slots_) {
            if (!s) ++free_slots;
            else if (detail::same_kind(*s, item)) room += kMaxStack - *detail::count_of(*s);
        }
        if (*count > room + free_slots * kMaxStack) return AddResult::Full;

        int remaining = *count;
        for (std::optional<Item>& s : slots_) {  // fill the existing stacks, lowest slot first
            if (remaining == 0) break;
            if (s && detail::same_kind(*s, item)) {
                const int take = std::min(remaining, kMaxStack - *detail::count_of(*s));
                *detail::count_of(*s) += take;
                remaining -= take;
            }
        }
        bool used_new_slot = false;
        while (remaining > 0) {
            const std::size_t at = *first_free();
            const int part = std::min(remaining, kMaxStack);
            remaining -= part;
            if (remaining == 0) {  // the last part can take the item itself
                *detail::count_of(item) = part;
                slots_[at] = std::move(item);
            } else {
                Item copy = item;
                *detail::count_of(copy) = part;
                slots_[at] = std::move(copy);
            }
            used_new_slot = true;
        }
        return used_new_slot ? AddResult::Added : AddResult::Stacked;
    }

    const Item* at(std::size_t slot) const { return slot < slots_.size() && slots_[slot] ? &*slots_[slot] : nullptr; }

    std::optional<Item> take(std::size_t slot) {
        if (slot >= slots_.size() || !slots_[slot]) return std::nullopt;
        std::optional<Item> out = std::move(slots_[slot]);
        slots_[slot].reset();
        return out;
    }

    UseResult use(std::size_t slot, Player& player) {
        if (slot >= slots_.size() || !slots_[slot]) return UseResult::NothingThere;
        std::optional<Item>& s = slots_[slot];
        return std::visit(detail::Overloaded{
                              [&](Potion& p) {
                                  if (player.hp >= player.max_hp) return UseResult::CannotUse;  // nothing to heal: keep the potion
                                  player.hp = std::min(player.max_hp, player.hp + p.heal);
                                  if (--p.count == 0) s.reset();
                                  return UseResult::Used;
                              },
                              [&](Weapon& w) {
                                  std::optional<Weapon> previous = std::move(player.weapon);
                                  player.weapon = std::move(w);
                                  if (previous) s = Item(std::move(*previous));
                                  else s.reset();
                                  return UseResult::Used;
                              },
                              [&](Armor& a) {
                                  std::optional<Armor> previous = std::move(player.armor);
                                  player.armor = std::move(a);
                                  if (previous) s = Item(std::move(*previous));
                                  else s.reset();
                                  return UseResult::Used;
                              },
                              [](Key&) { return UseResult::CannotUse; },
                              [](Gem&) { return UseResult::CannotUse; },
                          },
                          *s);
    }

    std::optional<std::size_t> find(std::string_view name) const {
        std::optional<std::size_t> last;
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i] && detail::name_of(*slots_[i]) == name) last = i;
        }
        if (last) return last;
        return std::nullopt;
    }

    long long total_value() const {
        long long total = 0;
        for (const std::optional<Item>& s : slots_) {
            if (const auto* g = s ? std::get_if<Gem>(&*s) : nullptr) total += static_cast<long long>(g->value) * g->count;
        }
        return total;
    }

private:
    std::vector<std::optional<Item>> slots_;

    std::optional<std::size_t> first_free() const {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i]) return i;
        }
        return std::nullopt;
    }
};

}  // namespace inv
