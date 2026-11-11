// MUTANT: updating an existing key returns true
#pragma once
// A std::list in recency order plus an unordered_map from key to list node: every operation is O(1) on average.
#include <cstddef>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

namespace lru {

template <class K, class V, class Hash = std::hash<K>>
class LruCache {
public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    V* get(const K& key) {
        const auto it = index_.find(key);
        if (it == index_.end()) {
            ++misses_;
            return nullptr;
        }
        ++hits_;
        items_.splice(items_.begin(), items_, it->second);
        return &it->second->second;
    }

    const V* peek(const K& key) const {
        const auto it = index_.find(key);
        return it == index_.end() ? nullptr : &it->second->second;
    }

    bool contains(const K& key) const { return index_.find(key) != index_.end(); }

    bool put(K key, V value) {
        if (capacity_ == 0) return false;
        const auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return true;
        }
        bool evicted = false;
        if (items_.size() == capacity_) {
            index_.erase(items_.back().first);
            items_.pop_back();
            evicted = true;
        }
        items_.emplace_front(std::move(key), std::move(value));
        index_.emplace(items_.front().first, items_.begin());
        return evicted;
    }

    bool erase(const K& key) {
        const auto it = index_.find(key);
        if (it == index_.end()) return false;
        items_.erase(it->second);
        index_.erase(it);
        return true;
    }

    void clear() {
        index_.clear();
        items_.clear();
    }

    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    std::size_t capacity() const { return capacity_; }
    std::size_t hits() const { return hits_; }
    std::size_t misses() const { return misses_; }

    template <class F>
    void for_each(F&& f) const {
        for (const auto& entry : items_) f(entry.first, entry.second);
    }

private:
    using List = std::list<std::pair<K, V>>;
    std::size_t capacity_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    List items_;  // front = most recently used
    std::unordered_map<K, typename List::iterator, Hash> index_;
};

}  // namespace lru
