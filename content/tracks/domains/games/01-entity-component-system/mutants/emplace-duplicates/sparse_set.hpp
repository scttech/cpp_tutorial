// MUTANT: emplace on an existing entity appends a second component
#pragma once
// A sparse set: dense arrays for iteration, a sparse array (indexed by entity) for O(1) lookup, swap-and-pop for O(1) removal.
#include <cstddef>
#include <utility>
#include <vector>

#include "types.hpp"

namespace ecs {

template <class T>
class SparseSet {
public:
    // the dense slot must point back at the entity: that is what makes stale sparse entries harmless
    bool contains(EntityId id) const { return id < sparse_.size() && sparse_[id] < dense_.size() && dense_[sparse_[id]] == id; }

    T& emplace(EntityId id, T value) {
        if (id >= sparse_.size()) sparse_.resize(static_cast<std::size_t>(id) + 1, kAbsent);
        sparse_[id] = dense_.size();
        dense_.push_back(id);
        components_.push_back(std::move(value));
        return components_.back();
    }

    bool erase(EntityId id) {
        if (!contains(id)) return false;
        const std::size_t slot = sparse_[id];
        const std::size_t last = dense_.size() - 1;
        if (slot != last) {  // the last component moves into the hole
            dense_[slot] = dense_[last];
            components_[slot] = std::move(components_[last]);
            sparse_[dense_[slot]] = slot;
        }
        dense_.pop_back();
        components_.pop_back();
        return true;
    }

    T* find(EntityId id) { return contains(id) ? &components_[sparse_[id]] : nullptr; }
    const T* find(EntityId id) const { return contains(id) ? &components_[sparse_[id]] : nullptr; }

    std::size_t size() const { return dense_.size(); }
    const std::vector<EntityId>& entities() const { return dense_; }
    std::vector<T>& components() { return components_; }
    const std::vector<T>& components() const { return components_; }

    void clear() {
        dense_.clear();
        components_.clear();
        sparse_.clear();
    }

private:
    static constexpr std::size_t kAbsent = ~std::size_t{0};
    std::vector<std::size_t> sparse_;  // entity -> slot in the dense arrays
    std::vector<EntityId> dense_;      // slot -> entity
    std::vector<T> components_;        // slot -> component
};

}  // namespace ecs
