#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <map>
#include <memory_resource>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "tracking_resource.hpp"
#include "zth_test.hpp"

using memtrack::TrackingResource;

namespace {

// An upstream that checks that it is used correctly, and remembers what it was asked.
class FakeUpstream : public std::pmr::memory_resource {
public:
    struct Call {
        std::size_t bytes, alignment;
    };
    std::vector<Call> allocs, frees;
    std::unordered_map<void*, Call> live;
    std::size_t misuse = 0;  // frees of unknown pointers or with another size/alignment
    long fail_at = -1;       // throw on the allocation with this index (0-based), if exceptions exist
    long calls = 0;

    ~FakeUpstream() override {
        for (auto& kv : live) ::operator delete(kv.first, std::align_val_t(kv.second.alignment));
    }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
#ifdef __cpp_exceptions
        if (fail_at >= 0 && calls++ == fail_at) throw std::bad_alloc();
#endif
        void* p = ::operator new(bytes == 0 ? 1 : bytes, std::align_val_t(alignment));
        allocs.push_back({bytes, alignment});
        live[p] = {bytes, alignment};
        return p;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        const auto it = live.find(p);
        if (it == live.end()) {
            ++misuse;
            return;
        }
        if (it->second.bytes != bytes || it->second.alignment != alignment) ++misuse;
        frees.push_back({bytes, alignment});
        ::operator delete(p, std::align_val_t(it->second.alignment));
        live.erase(it);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};

bool aligned(void* p, std::size_t a) { return reinterpret_cast<std::uintptr_t>(p) % a == 0; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a new tracker has empty books and says so") {
    FakeUpstream up;
    TrackingResource t(&up);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{0});
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{0});
    ZTH_EXPECT_EQ(t.total_allocations(), std::size_t{0});
    ZTH_EXPECT_EQ(t.total_deallocations(), std::size_t{0});
    ZTH_EXPECT_EQ(t.bad_frees(), std::size_t{0});
    ZTH_EXPECT_EQ(t.mismatched_frees(), std::size_t{0});
    ZTH_EXPECT_TRUE(t.leaks().empty());
    ZTH_EXPECT_EQ(t.report(), std::string("no leaks (0 allocations, peak 0 bytes)\n"));
}
ZTH_TEST("works", "allocate and deallocate update the counters") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(100, 16);
    void* b = t.allocate(24, 8);
    ZTH_EXPECT_TRUE(aligned(a, 16));
    ZTH_EXPECT_TRUE(aligned(b, 8));
    ZTH_EXPECT_TRUE(a != b);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{2});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{124});
    ZTH_EXPECT_EQ(t.total_allocations(), std::size_t{2});
    t.deallocate(a, 100, 16);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{1});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{24});
    t.deallocate(b, 24, 8);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{0});
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{124});
    ZTH_EXPECT_EQ(t.total_allocations(), std::size_t{2});
    ZTH_EXPECT_EQ(t.total_deallocations(), std::size_t{2});
    ZTH_EXPECT_EQ(t.bad_frees(), std::size_t{0});
    ZTH_EXPECT_EQ(t.mismatched_frees(), std::size_t{0});
}
ZTH_TEST("works", "the peak is the maximum ever, not the last value") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(100, 8);
    void* b = t.allocate(50, 8);
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{150});
    t.deallocate(a, 100, 8);
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{150});
    void* c = t.allocate(20, 8);
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{70});
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{150});
    void* d = t.allocate(200, 8);
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{270});
    t.deallocate(b, 50, 8);
    t.deallocate(c, 20, 8);
    t.deallocate(d, 200, 8);
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{270});
}
ZTH_TEST("works", "leaks() lists what is still allocated, in allocation order, with ids that start at 1") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(10, 1);
    void* b = t.allocate(20, 4);
    void* c = t.allocate(30, 8);
    t.deallocate(b, 20, 4);
    const auto leaks = t.leaks();
    ZTH_EXPECT_EQ(leaks.size(), std::size_t{2});
    if (leaks.size() != 2) return;
    ZTH_EXPECT_TRUE(leaks[0].ptr == a);
    ZTH_EXPECT_EQ(leaks[0].bytes, std::size_t{10});
    ZTH_EXPECT_EQ(leaks[0].alignment, std::size_t{1});
    ZTH_EXPECT_EQ(leaks[0].id, std::size_t{1});
    ZTH_EXPECT_TRUE(leaks[1].ptr == c);
    ZTH_EXPECT_EQ(leaks[1].bytes, std::size_t{30});
    ZTH_EXPECT_EQ(leaks[1].alignment, std::size_t{8});
    ZTH_EXPECT_EQ(leaks[1].id, std::size_t{3});
    t.deallocate(a, 10, 1);
    t.deallocate(c, 30, 8);
    ZTH_EXPECT_TRUE(t.leaks().empty());
}
ZTH_TEST("works", "ids are never reused, even after a free") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(8, 8);
    t.deallocate(a, 8, 8);
    void* b = t.allocate(8, 8);
    void* c = t.allocate(8, 8);
    const auto leaks = t.leaks();
    ZTH_EXPECT_EQ(leaks.size(), std::size_t{2});
    if (leaks.size() != 2) return;
    ZTH_EXPECT_EQ(leaks[0].id, std::size_t{2});
    ZTH_EXPECT_EQ(leaks[1].id, std::size_t{3});
    t.deallocate(b, 8, 8);
    t.deallocate(c, 8, 8);
}
ZTH_TEST("works", "the report when nothing leaked") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(100, 8);
    void* b = t.allocate(60, 8);
    t.deallocate(a, 100, 8);
    t.deallocate(b, 60, 8);
    ZTH_EXPECT_EQ(t.report(), std::string("no leaks (2 allocations, peak 160 bytes)\n"));
}
ZTH_TEST("works", "the report when something leaked: the totals, then one line per leak in allocation order") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(64, 16);
    void* b = t.allocate(5, 1);
    void* c = t.allocate(48, 8);
    t.deallocate(b, 5, 1);
    (void)a;
    (void)c;
    ZTH_EXPECT_EQ(t.report(), std::string("LEAK: 2 allocation(s), 112 byte(s) not freed\n  #1: 64 bytes, alignment 16\n  #3: 48 bytes, alignment 8\n"));
}
ZTH_TEST("works", "containers on a tracker leave no leaks when they are destroyed, and the books balance") {
    FakeUpstream up;
    TrackingResource t(&up);
    {
        std::pmr::vector<int> v(&t);
        for (int i = 0; i < 1000; ++i) v.push_back(i);
        std::pmr::string s("a string that is longer than any small-string buffer could hold", &t);
        std::pmr::map<int, std::pmr::string> m(&t);
        for (int i = 0; i < 20; ++i) m[i] = std::pmr::string(40, 'x');
        ZTH_EXPECT_TRUE(t.live_count() > 3);
        ZTH_EXPECT_TRUE(t.live_bytes() >= 4000);
    }
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{0});
    ZTH_EXPECT_TRUE(t.total_allocations() > 10);
    ZTH_EXPECT_EQ(t.total_allocations(), t.total_deallocations());
    ZTH_EXPECT_TRUE(t.peak_bytes() >= 4000);
    ZTH_EXPECT_TRUE(t.leaks().empty());
    ZTH_EXPECT_EQ(up.allocs.size(), up.frees.size());
    ZTH_EXPECT_EQ(up.live.size(), std::size_t{0});
}
ZTH_TEST("works", "a container that outlives the check shows up as a leak") {
    FakeUpstream up;
    TrackingResource t(&up);
    auto* v = new std::pmr::vector<int>(&t);
    v->resize(500);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{1});
    ZTH_EXPECT_EQ(t.live_bytes(), 500 * sizeof(int));
    const auto leaks = t.leaks();
    if (leaks.size() == 1) ZTH_EXPECT_EQ(leaks[0].bytes, 500 * sizeof(int));
    delete v;
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
}
ZTH_TEST("works", "a deallocation of a pointer that is not live is a BAD FREE: counted, not forwarded, nothing changes") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(64, 8);
    int on_the_stack = 0;
    t.deallocate(&on_the_stack, 4, 4);
    ZTH_EXPECT_EQ(t.bad_frees(), std::size_t{1});
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{1});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{64});
    ZTH_EXPECT_EQ(t.total_deallocations(), std::size_t{0});
    ZTH_EXPECT_EQ(up.frees.size(), std::size_t{0});
    t.deallocate(a, 64, 8);
    t.deallocate(a, 64, 8);  // a double free
    ZTH_EXPECT_EQ(t.bad_frees(), std::size_t{2});
    ZTH_EXPECT_EQ(t.total_deallocations(), std::size_t{1});
    ZTH_EXPECT_EQ(up.frees.size(), std::size_t{1});
    ZTH_EXPECT_EQ(up.misuse, std::size_t{0});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{0});
}
ZTH_TEST("works", "a deallocation with the wrong size or alignment is counted, and the block is freed as it was allocated") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(64, 16);
    t.deallocate(a, 32, 16);
    ZTH_EXPECT_EQ(t.mismatched_frees(), std::size_t{1});
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{0});
    ZTH_EXPECT_EQ(t.total_deallocations(), std::size_t{1});
    ZTH_EXPECT_EQ(up.misuse, std::size_t{0});   // the upstream saw 64 / 16
    void* b = t.allocate(40, 32);
    t.deallocate(b, 40, 8);   // alignment only
    ZTH_EXPECT_EQ(t.mismatched_frees(), std::size_t{2});
    ZTH_EXPECT_EQ(up.misuse, std::size_t{0});
    void* c = t.allocate(40, 8);
    t.deallocate(c, 40, 8);
    ZTH_EXPECT_EQ(t.mismatched_frees(), std::size_t{2});
}
ZTH_TEST("works", "the report lists bad frees and mismatches after the leak part") {
    FakeUpstream up;
    TrackingResource t(&up);
    void* a = t.allocate(16, 8);
    t.deallocate(a, 8, 8);
    t.deallocate(a, 16, 8);
    ZTH_EXPECT_EQ(t.report(), std::string("no leaks (1 allocations, peak 16 bytes)\nBAD FREE: 1 deallocation(s) of unknown pointers\nMISMATCH: 1 deallocation(s) with the wrong size or alignment\n"));
    void* b = t.allocate(24, 8);
    (void)b;
    ZTH_EXPECT_EQ(t.report(), std::string("LEAK: 1 allocation(s), 24 byte(s) not freed\n  #2: 24 bytes, alignment 8\nBAD FREE: 1 deallocation(s) of unknown pointers\nMISMATCH: 1 deallocation(s) with the wrong size or alignment\n"));
}
ZTH_TEST("works", "two memory resources are equal only if they are the same object") {
    FakeUpstream up;
    TrackingResource t1(&up), t2(&up);
    ZTH_EXPECT_TRUE(t1.is_equal(t1));
    ZTH_EXPECT_FALSE(t1.is_equal(t2));
    ZTH_EXPECT_FALSE(t1.is_equal(up));
    ZTH_EXPECT_FALSE(t1.is_equal(*std::pmr::new_delete_resource()));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "the upstream sees exactly the sizes and alignments that were asked, once each, and gets every block back") {
    FakeUpstream up;
    TrackingResource t(&up);
    const std::size_t sizes[] = {1, 7, 8, 64, 100, 1000};
    const std::size_t aligns[] = {1, 2, 8, 16, 32, 64};
    std::vector<void*> blocks;
    for (int i = 0; i < 6; ++i) blocks.push_back(t.allocate(sizes[i], aligns[i]));
    ZTH_EXPECT_EQ(up.allocs.size(), std::size_t{6});
    for (int i = 0; i < 6 && up.allocs.size() == 6; ++i) {
        ZTH_EXPECT_EQ(up.allocs[static_cast<std::size_t>(i)].bytes, sizes[i]);
        ZTH_EXPECT_EQ(up.allocs[static_cast<std::size_t>(i)].alignment, aligns[i]);
        ZTH_EXPECT_TRUE(aligned(blocks[static_cast<std::size_t>(i)], aligns[i]));
    }
    for (int i = 5; i >= 0; --i) t.deallocate(blocks[static_cast<std::size_t>(i)], sizes[i], aligns[i]);
    ZTH_EXPECT_EQ(up.frees.size(), std::size_t{6});
    ZTH_EXPECT_EQ(up.misuse, std::size_t{0});
    ZTH_EXPECT_EQ(up.live.size(), std::size_t{0});
}
#ifdef __cpp_exceptions
ZTH_TEST("robust", "when the upstream throws, the tracker records nothing and the next allocation gets id 1") {
    FakeUpstream up;
    up.fail_at = 1;
    TrackingResource t(&up);
    void* a = t.allocate(10, 8);   // call 0: fine
    bool threw = false;
    try {
        (void)t.allocate(99, 8);   // call 1: throws
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    ZTH_EXPECT_TRUE(threw);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{1});
    ZTH_EXPECT_EQ(t.live_bytes(), std::size_t{10});
    ZTH_EXPECT_EQ(t.peak_bytes(), std::size_t{10});
    ZTH_EXPECT_EQ(t.total_allocations(), std::size_t{1});
    void* b = t.allocate(20, 8);
    const auto leaks = t.leaks();
    ZTH_EXPECT_EQ(leaks.size(), std::size_t{2});
    if (leaks.size() == 2) ZTH_EXPECT_EQ(leaks[1].id, std::size_t{2});
    t.deallocate(a, 10, 8);
    t.deallocate(b, 20, 8);
}
#endif
ZTH_TEST("robust", "trackers can be stacked: the outer one sees the same blocks, and both report the leak") {
    FakeUpstream up;
    TrackingResource outer(&up);
    TrackingResource inner(&outer);
    void* a = inner.allocate(48, 16);
    void* b = inner.allocate(16, 8);
    ZTH_EXPECT_EQ(outer.live_count(), std::size_t{2});
    ZTH_EXPECT_EQ(outer.live_bytes(), std::size_t{64});
    inner.deallocate(a, 48, 16);
    ZTH_EXPECT_EQ(outer.live_count(), std::size_t{1});
    ZTH_EXPECT_EQ(inner.report(), std::string("LEAK: 1 allocation(s), 16 byte(s) not freed\n  #2: 16 bytes, alignment 8\n"));
    ZTH_EXPECT_EQ(outer.report(), std::string("LEAK: 1 allocation(s), 16 byte(s) not freed\n  #2: 16 bytes, alignment 8\n"));
    inner.deallocate(b, 16, 8);
    ZTH_EXPECT_EQ(outer.live_count(), std::size_t{0});
}
ZTH_TEST("robust", "a monitor sits under a monotonic buffer: the tracker sees the big chunks, and releasing them balances the books") {
    FakeUpstream up;
    TrackingResource t(&up);
    {
        std::pmr::monotonic_buffer_resource arena(1024, &t);
        std::pmr::vector<int> v(&arena);
        for (int i = 0; i < 5000; ++i) v.push_back(i);
        ZTH_EXPECT_TRUE(t.live_count() >= 1);
        ZTH_EXPECT_TRUE(t.total_allocations() < 30);   // chunks, not one allocation per element
    }
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(t.bad_frees(), std::size_t{0});
    ZTH_EXPECT_EQ(up.misuse, std::size_t{0});
}
ZTH_TEST("robust", "nested allocator-aware containers propagate the resource: not a single byte goes around the tracker") {
    FakeUpstream up;
    TrackingResource t(&up);
    {
        std::pmr::vector<std::pmr::string> names(&t);
        std::pmr::unordered_map<std::pmr::string, std::pmr::vector<int>> index(&t);
        for (int i = 0; i < 100; ++i) {
            names.emplace_back("a name that is long enough to need heap memory " + std::to_string(i));
            index[names.back()].assign(30, i);
        }
        std::size_t forwarded = 0, freed = 0;
        for (const auto& c : up.allocs) forwarded += c.bytes;
        for (const auto& c : up.frees) freed += c.bytes;
        const std::size_t balance = t.live_bytes() + freed;
        ZTH_EXPECT_EQ(forwarded, balance);
    }
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(up.live.size(), std::size_t{0});
}
ZTH_TEST("robust", "20000 random allocations and frees agree with a model: counters, peak, and the sorted leak list") {
    FakeUpstream up;
    TrackingResource t(&up);
    struct Block {
        void* p;
        std::size_t bytes, alignment, id;
    };
    std::vector<Block> live;
    std::size_t live_bytes = 0, peak = 0, allocs = 0, frees = 0;
    zth::Lcg rng(2024);
    const std::size_t aligns[] = {1, 2, 4, 8, 16, 32, 64};
    for (int step = 0; step < 20000; ++step) {
        if (live.empty() || rng.below(100) < 55) {
            const std::size_t bytes = static_cast<std::size_t>(1 + rng.below(300));
            const std::size_t alignment = aligns[rng.below(7)];
            void* p = t.allocate(bytes, alignment);
            if (!aligned(p, alignment)) {
                zth::fail("misaligned block at step " + std::to_string(step));
                return;
            }
            live.push_back({p, bytes, alignment, ++allocs});
            live_bytes += bytes;
            if (live_bytes > peak) peak = live_bytes;
        } else {
            const std::size_t i = static_cast<std::size_t>(rng.below(static_cast<int>(live.size())));
            t.deallocate(live[i].p, live[i].bytes, live[i].alignment);
            live_bytes -= live[i].bytes;
            ++frees;
            live[i] = live.back();
            live.pop_back();
        }
        if (step % 500 == 0 &&
            (t.live_count() != live.size() || t.live_bytes() != live_bytes || t.peak_bytes() != peak || t.total_allocations() != allocs || t.total_deallocations() != frees)) {
            zth::fail("the counters drifted from the model at step " + std::to_string(step));
            return;
        }
    }
    ZTH_EXPECT_EQ(t.live_count(), live.size());
    ZTH_EXPECT_EQ(t.live_bytes(), live_bytes);
    ZTH_EXPECT_EQ(t.peak_bytes(), peak);
    const auto leaks = t.leaks();
    ZTH_EXPECT_EQ(leaks.size(), live.size());
    std::map<std::size_t, Block> by_id;
    for (const Block& b : live) by_id[b.id] = b;
    std::size_t i = 0;
    for (const auto& kv : by_id) {
        if (i >= leaks.size()) break;
        if (leaks[i].id != kv.first || leaks[i].ptr != kv.second.p || leaks[i].bytes != kv.second.bytes || leaks[i].alignment != kv.second.alignment) {
            zth::fail("leak #" + std::to_string(i) + " is wrong");
            return;
        }
        ++i;
    }
    for (const Block& b : live) t.deallocate(b.p, b.bytes, b.alignment);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "bookkeeping is O(1) per call: 200000 live blocks allocated and freed in shuffled order, well within the timeout", 20) {
    TrackingResource t(std::pmr::new_delete_resource());
    constexpr int kBlocks = 200000;
    std::vector<void*> blocks;
    blocks.reserve(kBlocks);
    for (int i = 0; i < kBlocks; ++i) blocks.push_back(t.allocate(16, 8));
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{kBlocks});
    zth::Lcg rng(5);
    for (int i = kBlocks - 1; i > 0; --i) std::swap(blocks[static_cast<std::size_t>(i)], blocks[static_cast<std::size_t>(rng.below(i + 1))]);
    for (void* p : blocks) t.deallocate(p, 16, 8);
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
    ZTH_EXPECT_EQ(t.total_deallocations(), std::size_t{kBlocks});
    ZTH_EXPECT_EQ(t.bad_frees(), std::size_t{0});
}
ZTH_TEST("fast", "the tracker's own books cost at most one heap allocation per tracked block (the upstream here is a plain buffer, so every counted allocation is the tracker's)") {
    alignas(64) static char storage[256 * 1024];
    std::pmr::monotonic_buffer_resource arena(storage, sizeof storage, std::pmr::null_memory_resource());
    TrackingResource t(&arena);
    constexpr int kBlocks = 2000;
    std::vector<void*> blocks;
    blocks.reserve(kBlocks);
    zth::mem::Scope scope;
    for (int i = 0; i < kBlocks; ++i) blocks.push_back(t.allocate(32, 8));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, kBlocks + 64);   // one hash-map node per block, plus a few bucket arrays
    for (void* p : blocks) t.deallocate(p, 32, 8);
}
ZTH_TEST_TIMEOUT("fast", "a report of 30000 leaks is produced at once, in order", 20) {
    TrackingResource t(std::pmr::new_delete_resource());
    std::vector<void*> blocks;
    for (int i = 0; i < 30000; ++i) blocks.push_back(t.allocate(static_cast<std::size_t>(1 + i % 50), 8));
    const std::string r = t.report();
    ZTH_EXPECT_TRUE(r.rfind("LEAK: 30000 allocation(s),", 0) == 0);
    ZTH_EXPECT_TRUE(r.find("  #1: 1 bytes, alignment 8\n  #2: 2 bytes, alignment 8\n") != std::string::npos);
    ZTH_EXPECT_TRUE(r.find("  #30000: 50 bytes, alignment 8\n") != std::string::npos);
    for (void* p : blocks) t.deallocate(p, 1, 8);   // wrong sizes on purpose: freed as allocated
    ZTH_EXPECT_EQ(t.live_count(), std::size_t{0});
}
ZTH_TEST("fast", "no padding and no headers: the bytes that reach the upstream equal the bytes that were asked") {
    FakeUpstream up;
    TrackingResource t(&up);
    std::size_t asked = 0;
    std::vector<std::pair<void*, std::size_t>> blocks;
    for (int i = 1; i <= 50; ++i) {
        const std::size_t n = static_cast<std::size_t>(i * 3);
        blocks.push_back({t.allocate(n, 8), n});
        asked += n;
    }
    std::size_t got = 0;
    for (const auto& c : up.allocs) got += c.bytes;
    ZTH_EXPECT_EQ(got, asked);
    for (const auto& b : blocks) t.deallocate(b.first, b.second, 8);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("tracking_resource.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tracking_resource.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the books live in ordinary containers, not in the upstream: no pmr containers inside the tracker") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tracking_resource.hpp"));
    for (const char* word : {"polymorphic_allocator", "monotonic_buffer_resource", "unsynchronized_pool_resource", "synchronized_pool_resource"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    for (const char* word : {"printf", "cout", "cerr", "fprintf", "puts"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
