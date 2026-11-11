// MUTANT: lookup on an empty ring reads a point that is not there
#pragma once
// A sorted vector of points; lookup is a binary search with wrap-around; adding a node merges its sorted points in.
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
    struct Point {
        std::uint64_t hash;
        const std::string* node;  // points at the key of nodes_: stable while the node exists
    };
    static bool before(const Point& a, const Point& b) { return a.hash < b.hash || (a.hash == b.hash && *a.node < *b.node); }

public:
    explicit HashRing(std::size_t vnodes_per_weight = 100) : vnodes_(vnodes_per_weight == 0 ? 1 : vnodes_per_weight) {}

    bool add_node(std::string_view name, std::size_t weight = 1) {
        if (weight == 0 || contains(name)) return false;
        const auto it = nodes_.emplace(std::string(name), weight).first;
        const std::string* node = &it->first;
        const std::size_t count = vnodes_ * weight;
        std::vector<Point> fresh;
        fresh.reserve(count);
        for (std::size_t i = 0; i < count; ++i) fresh.push_back(Point{hash64(vnode_label(name, i)), node});
        std::sort(fresh.begin(), fresh.end(), before);
        const std::size_t old_size = points_.size();
        points_.insert(points_.end(), fresh.begin(), fresh.end());
        std::inplace_merge(points_.begin(), points_.begin() + static_cast<std::ptrdiff_t>(old_size), points_.end(), before);
        return true;
    }

    bool remove_node(std::string_view name) {
        const auto it = nodes_.find(std::string(name));
        if (it == nodes_.end()) return false;
        const std::string* node = &it->first;
        points_.erase(std::remove_if(points_.begin(), points_.end(), [node](const Point& p) { return p.node == node; }), points_.end());
        nodes_.erase(it);
        return true;
    }

    bool contains(std::string_view name) const { return nodes_.find(std::string(name)) != nodes_.end(); }
    std::size_t node_count() const { return nodes_.size(); }
    std::size_t point_count() const { return points_.size(); }
    std::size_t vnodes_per_weight() const { return vnodes_; }

    std::vector<std::string> nodes() const {
        std::vector<std::string> out;
        out.reserve(nodes_.size());
        for (const auto& entry : nodes_) out.push_back(entry.first);
        std::sort(out.begin(), out.end());
        return out;
    }

    const std::string* lookup(std::string_view key) const {
        return points_[first_point_at_or_after(hash64(key))].node;
    }

    std::vector<std::string> lookup_n(std::string_view key, std::size_t n) const {
        std::vector<std::string> out;
        if (points_.empty() || n == 0) return out;
        n = std::min(n, nodes_.size());
        out.reserve(n);
        const std::size_t start = first_point_at_or_after(hash64(key));
        for (std::size_t step = 0; step < points_.size() && out.size() < n; ++step) {
            const std::string& node = *points_[(start + step) % points_.size()].node;
            if (std::find(out.begin(), out.end(), node) == out.end()) out.push_back(node);
        }
        return out;
    }

private:
    // Index of the first point with hash >= h; wraps to 0 past the last point. Requires a non-empty ring.
    std::size_t first_point_at_or_after(std::uint64_t h) const {
        auto it = std::lower_bound(points_.begin(), points_.end(), h, [](const Point& p, std::uint64_t value) { return p.hash < value; });
        if (it == points_.end()) it = points_.begin();
        return static_cast<std::size_t>(it - points_.begin());
    }

    std::size_t vnodes_;
    std::vector<Point> points_;                        // sorted by (hash, node name)
    std::unordered_map<std::string, std::size_t> nodes_;  // node name -> weight
};

}  // namespace chash
