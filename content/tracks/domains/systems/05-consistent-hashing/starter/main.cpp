// A cache cluster: which server owns which key, and how little moves when a server joins or leaves. Read-only. Press Run.
#include <cstdio>
#include <map>
#include <string>

#include "hash_ring.hpp"
#include "types.hpp"

int main() {
    chash::HashRing ring;   // 100 virtual nodes per unit of weight
    for (const char* server : {"cache-a", "cache-b", "cache-c", "cache-d"}) ring.add_node(server);
    ring.add_node("cache-big", 2);   // twice the capacity: twice the points

    const int kKeys = 20000;
    std::map<std::string, int> load;
    std::map<int, std::string> before;
    for (int i = 0; i < kKeys; ++i) {
        const std::string* owner = ring.lookup("user:" + std::to_string(i));
        ++load[*owner];
        before[i] = *owner;
    }
    std::printf("%zu servers, %zu points on the ring\n", ring.node_count(), ring.point_count());
    for (const auto& kv : load) std::printf("  %-10s owns %5d keys (%4.1f%%)\n", kv.first.c_str(), kv.second, 100.0 * kv.second / kKeys);

    ring.add_node("cache-e");
    int moved = 0;
    for (int i = 0; i < kKeys; ++i) moved += *ring.lookup("user:" + std::to_string(i)) != before[i];
    std::printf("after adding cache-e: %d of %d keys moved (%.1f%%); a modulo scheme would move about 83%%\n", moved, kKeys, 100.0 * moved / kKeys);

    const std::string key = "user:42";
    std::printf("replicas of %s:", key.c_str());
    for (const std::string& node : ring.lookup_n(key, 3)) std::printf(" %s", node.c_str());
    std::printf("\n");
}
