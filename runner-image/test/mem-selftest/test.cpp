// Self-test for the allocation tracking and Tracked helpers in zth_test.hpp.
// Every "expect-*" test below must produce the status named in its title, using deliberately buggy code.
#define ZTH_TRACK_ALLOCATIONS
#include <memory>
#include <vector>

#include "zth_test.hpp"

ZTH_TEST("works", "clean: balanced new/delete passes") {
    zth::mem::Scope m;
    int* p = new int(5);
    delete p;
    int* a = new int[10];
    delete[] a;
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}

ZTH_TEST("works", "clean: std containers and smart pointers balance") {
    zth::mem::Scope m;
    {
        std::vector<int> v(100, 1);
        v.push_back(2);
        auto u = std::make_unique<int>(3);
        auto s = std::make_shared<std::vector<int>>(10);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}

ZTH_TEST("works", "expect-fail: a leak is reported with its size") {
    zth::mem::Scope m;
    int* p = new int[25];
    (void)p;
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}

ZTH_TEST("works", "expect-fail: double free is reported, not a crash") {
    zth::mem::Scope m;
    int* p = new int(1);
    delete p;
    delete p;
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}

ZTH_TEST("works", "expect-fail: new[] released with delete is reported") {
    zth::mem::Scope m;
    int* p = new int[4];
    delete p;
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}

ZTH_TEST("works", "expect-fail: delete of a stack address is reported, not a crash") {
    zth::mem::Scope m;
    int x = 0;
    // The pointer is not from new. We cannot always read before it safely, but the header read is on the stack here.
    char buf[64] = {};
    delete reinterpret_cast<int*>(buf + 32);
    (void)x;
    ZTH_EXPECT_NO_MEMORY_BUGS(m);
}

ZTH_TEST("works", "use after free reads poison (0xDD), not the old value") {
    int* p = new int(42);
    delete p;
    volatile unsigned char first = *reinterpret_cast<unsigned char*>(p);  // deliberate use-after-free
    ZTH_EXPECT_EQ(static_cast<int>(first), 0xDD);
}

ZTH_TEST("works", "alloc counting: reserve avoids reallocation") {
    std::vector<int> v;
    zth::mem::Scope m;
    v.reserve(100);
    for (int i = 0; i < 100; ++i) v.push_back(i);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 1);
}

ZTH_TEST("works", "expect-fail: alloc limit is enforced") {
    std::vector<int> v;
    zth::mem::Scope m;
    for (int i = 0; i < 100; ++i) v.push_back(i);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 1);
}

ZTH_TEST("works", "Tracked: vector growth moves (noexcept move), no copies") {
    zth::Tracked::reset();
    {
        std::vector<zth::Tracked> v;
        for (int i = 0; i < 8; ++i) v.emplace_back(i);
        ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
        ZTH_EXPECT_TRUE(zth::Tracked::moves > 0);
    }
    ZTH_EXPECT_EQ(zth::Tracked::live(), 0);
}

ZTH_TEST("works", "source scan: finds new/delete/malloc/free in code") {
    auto u = zth::raw_memory_uses("int* p = new int(3);\ndelete p;\nvoid* q = malloc(4);\nfree(q);\n");
    ZTH_EXPECT_EQ(u, (std::vector<std::string>{"new", "delete", "malloc", "free"}));
}
ZTH_TEST("works", "source scan: ignores comments, strings, = delete, and look-alike identifiers") {
    const std::string src =
        "// we never use new or delete here\n"
        "/* malloc(1); free(p); */\n"
        "const char* s = \"new delete \\\" malloc\";\n"
        "char c = '\\'';\n"
        "struct A { A(const A&) = delete; };\n"
        "int renew = 1, news = 2, freed = 3; void free_list();\n"
        "int free = 4;\n";
    ZTH_EXPECT_EQ(zth::raw_memory_uses(src), std::vector<std::string>{});
}
ZTH_TEST("works", "source scan: comment markers inside strings do not hide code") {
    auto u = zth::raw_memory_uses("const char* s = \"//\"; int* p = new int;\n");
    ZTH_EXPECT_EQ(u, std::vector<std::string>{"new"});
}
ZTH_TEST("works", "dirty_stack makes an uninitialized local visible (0x5A5A5A5A), not zero") {
    struct Probe {
        __attribute__((noinline)) static int read_uninitialized() {
            int x;  // deliberately uninitialized
            return *reinterpret_cast<volatile int*>(&x);
        }
    };
    zth::dirty_stack();
    ZTH_EXPECT_EQ(Probe::read_uninitialized(), 0x5A5A5A5A);
}
ZTH_TEST("works", "leaked_blocks never goes negative when pre-existing blocks are freed in the scope") {
    int* old = new int(1);
    zth::mem::Scope m;
    delete old;
    ZTH_EXPECT_EQ(m.leaked_blocks(), std::size_t{0});
}

ZTH_TEST("works", "Tracked: copying a vector copies every element") {
    std::vector<zth::Tracked> v{1, 2, 3};
    zth::Tracked::reset();
    std::vector<zth::Tracked> w = v;
    ZTH_EXPECT_EQ(zth::Tracked::copies, 3);
}
