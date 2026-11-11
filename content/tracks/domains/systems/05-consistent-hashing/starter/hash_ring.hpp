#pragma once
// YOUR CONSISTENT-HASHING RING: decide which server owns a key, so that adding or removing a server moves only a small fraction of the keys.
//
//   HashRing(vnodes_per_weight = 100)      0 is treated as 1
//   bool add_node(name, weight = 1)        false if `name` exists or weight == 0. A node of weight w puts   vnodes_per_weight * w   points on the ring:
//                                          point i sits at  hash64(vnode_label(name, i))   (see types.hpp)
//   bool remove_node(name)                 false if unknown; removes all of the node's points
//   bool contains(name) const              node_count()   point_count()   nodes() (names in ALPHABETICAL order)
//   const std::string* lookup(key) const   the owner: the node of the first point at or after hash64(key), going clockwise and WRAPPING
//                                          from the last point to the first; nullptr if the ring is empty. The pointer stays valid until that node is removed.
//   std::vector<std::string> lookup_n(key, n) const
//                                          the first n DISTINCT nodes met going clockwise from the key's position (the owner first): replicas.
//                                          Fewer if the ring has fewer nodes; empty for n == 0 or an empty ring.
//
// How: a std::vector<Point{hash, node}> kept SORTED by (hash, node name), and std::lower_bound for lookups: O(log points), no allocation.
// Adding a node: build its points, sort THEM (a few hundred), append and std::inplace_merge: O(points) instead of re-sorting everything.
// Removing: std::remove_if keeps the order. Nodes live in an unordered_map<string, weight>: its keys never move, so Point can hold a pointer to the name.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace chash {

class HashRing {
public:
    explicit HashRing(std::size_t vnodes_per_weight = 100) : vnodes_(vnodes_per_weight == 0 ? 1 : vnodes_per_weight) {}

    bool add_node(std::string_view name, std::size_t weight = 1) {
        (void)name;
        (void)weight;
        return false;  // TODO
    }
    bool remove_node(std::string_view name) {
        (void)name;
        return false;  // TODO
    }
    bool contains(std::string_view name) const {
        (void)name;
        return false;  // TODO
    }
    std::size_t node_count() const { return 0; }   // TODO
    std::size_t point_count() const { return 0; }  // TODO
    std::vector<std::string> nodes() const { return {}; }  // TODO
    std::size_t vnodes_per_weight() const { return vnodes_; }

    const std::string* lookup(std::string_view key) const {
        (void)key;
        return nullptr;  // TODO
    }
    std::vector<std::string> lookup_n(std::string_view key, std::size_t n) const {
        (void)key;
        (void)n;
        return {};  // TODO
    }

private:
    std::size_t vnodes_;
    // TODO: the sorted points and the node table
};

}  // namespace chash
