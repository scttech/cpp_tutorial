#pragma once
// YOUR COMPONENT STORE: a sparse set. All the components of one type live together in a dense array (good for iteration);
// a second array, indexed by entity, says where each entity's component is. Add and look up in O(1); remove in O(1) by
// moving the LAST component into the hole (swap and pop), not by shifting everything.
//
//   contains(id)        has this entity a component here?
//   emplace(id, value)  add the component; if the entity already has one, replace its value (no duplicate); returns a reference to it
//   erase(id)           remove it (true) or do nothing (false)
//   find(id)            pointer to the component, or nullptr
//   size(), entities(), components()   the dense arrays, in the same order: entities()[i] owns components()[i]
//   clear()
#include <cstddef>
#include <utility>
#include <vector>

#include "types.hpp"

namespace ecs {

template <class T>
class SparseSet {
public:
    bool contains(EntityId id) const {
        (void)id;
        return false;  // TODO
    }
    T& emplace(EntityId id, T value) {
        (void)id;
        components_.push_back(std::move(value));  // TODO: the sparse array, the dense entity list, replacing
        return components_.back();
    }
    bool erase(EntityId id) {
        (void)id;
        return false;  // TODO: swap and pop
    }
    T* find(EntityId) { return nullptr; }               // TODO
    const T* find(EntityId) const { return nullptr; }   // TODO
    std::size_t size() const { return components_.size(); }
    const std::vector<EntityId>& entities() const { return dense_; }
    std::vector<T>& components() { return components_; }
    const std::vector<T>& components() const { return components_; }
    void clear() {
        dense_.clear();
        components_.clear();
    }

private:
    std::vector<EntityId> dense_;
    std::vector<T> components_;
    // TODO: the sparse array
};

}  // namespace ecs
