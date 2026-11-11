// MUTANT: each runs even when a store is missing (crashes)
#pragma once
// Registry: generational entity handles and one type-erased SparseSet per component type.
#include <array>
#include <cstddef>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sparse_set.hpp"
#include "types.hpp"

namespace ecs {
namespace detail {

// what the registry can do with a store without knowing its component type
struct StoreBase {
    virtual ~StoreBase() = default;
    virtual bool erase(EntityId id) = 0;
    virtual std::size_t size() const = 0;
    virtual const std::vector<EntityId>& entities() const = 0;
};

template <class T>
struct Store final : StoreBase {
    SparseSet<T> set;
    bool erase(EntityId id) override { return set.erase(id); }
    std::size_t size() const override { return set.size(); }
    const std::vector<EntityId>& entities() const override { return set.entities(); }
};

}  // namespace detail

class Registry {
public:
    Entity create() {
        if (!free_.empty()) {
            const EntityId slot = free_.back();
            free_.pop_back();
            return Entity{slot, generations_[slot]};  // the generation was bumped when the slot was freed
        }
        generations_.push_back(0);
        return Entity{static_cast<EntityId>(generations_.size() - 1), 0};
    }

    bool alive(Entity e) const { return e.index < generations_.size() && generations_[e.index] == e.generation; }

    void destroy(Entity e) {
        if (!alive(e)) return;
        for (auto& entry : stores_) entry.second->erase(e.index);
        ++generations_[e.index];  // every old handle to this slot is now stale
        free_.push_back(e.index);
    }

    template <class T>
    T* add(Entity e, T value) {
        if (!alive(e)) return nullptr;
        return &store<T>().set.emplace(e.index, std::move(value));
    }
    template <class T>
    T* get(Entity e) {
        auto* s = find_store<T>();
        return (s && alive(e)) ? s->set.find(e.index) : nullptr;
    }
    template <class T>
    bool has(Entity e) const {
        const auto* s = find_store<T>();
        return s && alive(e) && s->set.contains(e.index);
    }
    template <class T>
    bool remove(Entity e) {
        auto* s = find_store<T>();
        return s && alive(e) && s->set.erase(e.index);
    }
    template <class T>
    std::size_t count() const {
        const auto* s = find_store<T>();
        return s ? s->set.size() : 0;
    }

    template <class... Cs, class F>
    void each(F&& f) {
        const std::array<const detail::StoreBase*, sizeof...(Cs)> stores{find_store<Cs>()...};
        const detail::StoreBase* smallest = nullptr;
        for (const detail::StoreBase* s : stores) {
            if (s == nullptr) continue;
            if (smallest == nullptr || s->size() < smallest->size()) smallest = s;
        }
        if (smallest == nullptr) return;
        const std::vector<EntityId>& ids = smallest->entities();
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const EntityId id = ids[i];
            if (!(find_store<Cs>()->set.contains(id) && ...)) continue;
            f(Entity{id, generations_[id]}, *find_store<Cs>()->set.find(id)...);
        }
    }

private:
    std::vector<std::uint32_t> generations_;
    std::vector<EntityId> free_;
    std::unordered_map<std::type_index, std::unique_ptr<detail::StoreBase>> stores_;

    template <class T>
    detail::Store<T>& store() {
        std::unique_ptr<detail::StoreBase>& slot = stores_[std::type_index(typeid(T))];
        if (!slot) slot = std::make_unique<detail::Store<T>>();
        return static_cast<detail::Store<T>&>(*slot);
    }
    template <class T>
    detail::Store<T>* find_store() {
        const auto it = stores_.find(std::type_index(typeid(T)));
        return it == stores_.end() ? nullptr : static_cast<detail::Store<T>*>(it->second.get());
    }
    template <class T>
    const detail::Store<T>* find_store() const {
        const auto it = stores_.find(std::type_index(typeid(T)));
        return it == stores_.end() ? nullptr : static_cast<const detail::Store<T>*>(it->second.get());
    }
};

}  // namespace ecs
