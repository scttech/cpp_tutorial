#pragma once
// YOUR LRU CACHE: a fixed-capacity key/value cache that throws away the LEAST RECENTLY USED entry when it is full.
// Every operation must take O(1) time on average.
//
//   LruCache<K, V, Hash>(capacity)   capacity may be 0 (a cache that stores nothing)
//   V* get(key)                      the value or nullptr. A hit makes the entry the MOST recently used. Counts a hit or a miss.
//   const V* peek(key) const         the value or nullptr, WITHOUT changing the order and without counting
//   bool contains(key) const         the same: no promotion, no counting
//   bool put(key, value)             inserts or updates; the entry becomes the most recently used.
//                                    Returns true only if an OLD entry had to be evicted to make room (updating an existing key never evicts).
//   bool erase(key)                  true if it was present
//   void clear()                     removes all entries; the hit/miss counters are kept
//   size(), empty(), capacity(), hits(), misses()
//   for_each(f)                      calls f(key, value) for every entry, MOST recently used first
//
// How: a std::list<std::pair<K, V>> keeps the entries in recency order (front = most recent) and a std::unordered_map<K, list iterator>
// finds an entry in O(1). "Promote" = std::list::splice, which relinks the node WITHOUT copying or moving anything and without
// invalidating pointers or iterators to it. get() must return a pointer that stays valid until that entry is erased or evicted.
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
        (void)key;
        return nullptr;  // TODO
    }
    const V* peek(const K& key) const {
        (void)key;
        return nullptr;  // TODO
    }
    bool contains(const K& key) const {
        (void)key;
        return false;  // TODO
    }
    bool put(K key, V value) {
        (void)key;
        (void)value;
        return false;  // TODO
    }
    bool erase(const K& key) {
        (void)key;
        return false;  // TODO
    }
    void clear() {}  // TODO

    std::size_t size() const { return 0; }  // TODO
    bool empty() const { return true; }     // TODO
    std::size_t capacity() const { return capacity_; }
    std::size_t hits() const { return hits_; }
    std::size_t misses() const { return misses_; }

    template <class F>
    void for_each(F&& f) const {
        (void)f;  // TODO: most recently used first
    }

private:
    using List = std::list<std::pair<K, V>>;
    std::size_t capacity_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
    // TODO: List items_ (front = most recent) and std::unordered_map<K, typename List::iterator, Hash> index_
};

}  // namespace lru
