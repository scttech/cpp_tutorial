// Runs the pipeline on a made-up log of 5000 lines and prints what it cost (read-only). Press Run.
// (This file counts heap allocations with its own operator new.)
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>

static unsigned long long g_allocations = 0;
// (weak: the hidden tests bring their own counting operator new, and that one wins at link time)
__attribute__((weak)) void* operator new(std::size_t n) {
    ++g_allocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort();
}
__attribute__((weak)) void operator delete(void* p) noexcept { std::free(p); }
__attribute__((weak)) void operator delete(void* p, std::size_t) noexcept { std::free(p); }

#include "pipeline.hpp"
#include "types.hpp"

int main() {
    const std::vector<std::string> log = make_log(5000);
    probe::reset();
    g_allocations = 0;
    const Report report = analyze(log);
    std::cout << describe(report) << "\n\n";
    std::cout << "UserId comparisons: " << probe::compares << "\n";
    std::cout << "UserId copies:      " << probe::copies << "\n";
    std::cout << "heap allocations:   " << g_allocations << "\n";
    std::cout << "(" << log.size() << " lines, " << report.distinct_users << " users)\n";
}
