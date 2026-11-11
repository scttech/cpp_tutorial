# Fast path: what's different in C++

- **The workflow is the same in any language, the tools are native**: build with `-O2 -g -fno-omit-frame-pointer`, run a sampling profiler (`perf`, Instruments, VTune), find the hot function, look at its assembly (Compiler Explorer, `perf annotate`), fix the layout or algorithm, measure again.
- **C++ specifics to look for**: needless copies (`for (auto x : v)` of a big type), allocations in loops, virtual calls that stop inlining, `std::endl` flushing, `std::map` where a sorted vector would do, `shared_ptr` reference counting in hot loops, exceptions on hot paths.
- **Profile-guided optimization** (`-fprofile-generate` / `-fprofile-use`) and link-time optimization (`-flto`) let the compiler use real data; try them last.
- **Do not trust one number.** Pin the CPU frequency if you can, repeat the run, look at the median and the spread.
