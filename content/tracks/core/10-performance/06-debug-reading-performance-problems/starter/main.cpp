// The profile of the six functions (read-only). Press Run, read the numbers, find the function whose cost is out of proportion, fix it, run again.
// (This file counts heap allocations with its own operator new.)
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <new>
#include <string>
#include <vector>

static unsigned long long g_allocations = 0;
// (weak: the hidden tests bring their own counting operator new, and that one wins at link time)
__attribute__((weak)) void* operator new(std::size_t n) {
    ++g_allocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort();
}
__attribute__((weak)) void operator delete(void* p) noexcept { std::free(p); }
__attribute__((weak)) void operator delete(void* p, std::size_t) noexcept { std::free(p); }

#include "probe.hpp"
#include "slow.hpp"

static void row(const char* name, const char* what, unsigned long long a, unsigned long long b, unsigned long long c) {
    std::printf("%-14s %-16s %12llu %12llu %12llu\n", name, what, a, b, c);
}

int main() {
    std::printf("%-14s %-16s %12s %12s %12s\n", "function", "input", "copies", "moves", "compares");
    std::printf("%-14s %-16s %12s\n", "", "", "(heap allocations are shown as 'allocs' where they matter)");

    std::vector<Tracked> numbers;
    for (int i = 0; i < 2000; ++i) numbers.emplace_back(i);

    probe::reset();
    auto r = reversed(numbers);
    row("reversed", "2000 numbers", probe::copies, probe::moves, probe::compares);

    probe::reset();
    long long w = weight_sum(numbers);
    row("weight_sum", "2000 numbers", probe::copies, probe::moves, probe::compares);

    probe::reset();
    bool d = has_duplicates(numbers);
    row("has_duplicates", "2000 numbers", probe::copies, probe::moves, probe::compares);

    std::map<Tracked, int> table;
    for (int i = 0; i < 1024; ++i) table.emplace(Tracked(i), i);
    probe::reset();
    long long found = 0;
    for (int i = 0; i < 100; ++i) found += lookup_count(table, Tracked(i * 7));
    row("lookup_count", "100 lookups", probe::copies, probe::moves, probe::compares);

    std::vector<std::string> words;
    for (int i = 0; i < 1000; ++i) words.push_back("a-word-that-is-long-enough-" + std::to_string(i));
    g_allocations = 0;
    std::string j = join(words, ',');
    std::printf("%-14s %-16s %12llu allocs\n", "join", "1000 words", g_allocations);
    g_allocations = 0;
    std::string l = longest(words);
    std::printf("%-14s %-16s %12llu allocs\n", "longest", "1000 words", g_allocations);

    std::cout << "\n(results: " << r.size() << " " << w << " " << d << " " << found << " " << j.size() << " " << l.size() << ")\n";
}
