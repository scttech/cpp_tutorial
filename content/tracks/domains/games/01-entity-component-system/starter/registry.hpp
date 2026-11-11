#pragma once
// YOUR REGISTRY: creates and destroys entities, and keeps one SparseSet per component type.
//
//   create()                 a new entity. A destroyed entity's slot is reused, with generation + 1.
//   alive(e)                 is this handle current? (a stale handle, whose slot was reused, is not alive)
//   destroy(e)               remove ALL its components, free the slot (a stale or dead handle: nothing happens)
//   add<T>(e, value)         give e a component (replaces an existing one); nullptr if e is not alive; else a pointer to it
//   get<T>(e), has<T>(e), remove<T>(e)   (not alive: nullptr / false / false)
//   count<T>()               how many entities have a T
//   each<A, B, ...>(f)       call f(Entity, A&, B&, ...) for every entity that has ALL of them. Walk the SMALLEST of the stores.
//                            f must not add or remove components of those types while each runs.
#include <cstddef>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sparse_set.hpp"
#include "types.hpp"

namespace ecs {

class Registry {
public:
    Entity create() {
        generations_.push_back(0);
        return Entity{static_cast<EntityId>(generations_.size() - 1), 0};  // TODO: reuse destroyed slots
    }

    bool alive(Entity e) const { return e.index < generations_.size(); }  // TODO: the generation

    void destroy(Entity e) { (void)e; }  // TODO

    template <class T>
    T* add(Entity e, T value) {
        (void)e;
        (void)value;
        return nullptr;  // TODO: one SparseSet<T> per type (type erasure: the registry holds them behind a common base class)
    }
    template <class T>
    T* get(Entity) { return nullptr; }
    template <class T>
    bool has(Entity) const { return false; }
    template <class T>
    bool remove(Entity) { return false; }
    template <class T>
    std::size_t count() const { return 0; }

    template <class... Cs, class F>
    void each(F&& f) {
        (void)f;  // TODO
    }

private:
    std::vector<std::uint32_t> generations_;
};

}  // namespace ecs
