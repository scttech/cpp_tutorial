// A small heap of 1024 bytes and a few allocations; the stats after each step. Read-only. Press Run.
#include <cstdio>
#include <cstring>

#include "allocator.hpp"

static void show(const char* what, const alloc::Allocator& a) {
    const alloc::Stats s = a.stats();
    std::printf("%-28s used %zu blocks / %zu bytes, free %zu blocks / %zu bytes (largest %zu), check %s\n", what, s.used_blocks, s.used_bytes, s.free_blocks,
                s.free_bytes, s.largest_free, a.check() ? "ok" : "BROKEN");
}

int main() {
    alignas(16) static unsigned char memory[1024];
    alloc::Heap heap(memory, sizeof memory);
    alloc::Allocator a(heap);
    show("empty", a);

    void* p1 = a.allocate(100);
    void* p2 = a.allocate(200);
    void* p3 = a.allocate(50);
    show("three blocks", a);
    std::printf("payloads at offsets %td %td %td (aligned to 16: %s)\n", static_cast<std::byte*>(p1) - heap.base(), static_cast<std::byte*>(p2) - heap.base(),
                static_cast<std::byte*>(p3) - heap.base(), (reinterpret_cast<std::uintptr_t>(p1) % 16 == 0) ? "yes" : "NO");

    a.deallocate(p2);
    show("middle block freed", a);
    a.deallocate(p1);
    show("first block freed (merges)", a);
    a.deallocate(p3);
    show("all freed (one block)", a);

    void* big = a.allocate(a.stats().largest_free);
    std::printf("the whole heap in one block: %s\n", big ? "yes" : "no");
    show("after the big one", a);
    std::printf("a second request fails: %s\n", a.allocate(1) == nullptr ? "yes" : "no");
    a.deallocate(big);
    a.deallocate(big);
    show("after a double free", a);
    std::printf("rejected frees: %zu\n", a.stats().rejected_frees);
}
