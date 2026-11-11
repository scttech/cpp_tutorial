// A small page cache in front of a slow "disk". Read-only. Press Run.
#include <cstdio>
#include <string>

#include "lru.hpp"

int main() {
    lru::LruCache<int, std::string> cache(3);
    auto read_page = [&](int page) {
        if (const std::string* hit = cache.get(page)) {
            std::printf("page %d: hit  (%s)\n", page, hit->c_str());
        } else {
            std::printf("page %d: miss, reading from disk\n", page);
            cache.put(page, "data-" + std::to_string(page));
        }
    };
    for (int page : {1, 2, 3, 1, 4, 2, 5, 1}) read_page(page);
    std::printf("hits %zu, misses %zu, resident %zu of %zu\n", cache.hits(), cache.misses(), cache.size(), cache.capacity());
    std::printf("most recent first:");
    cache.for_each([](int page, const std::string&) { std::printf(" %d", page); });
    std::printf("\n");
}
