#pragma once

// READING A PROFILE. Three tools looked at the same program (a batch image thumbnailer). Their output is in the files next to this one:
//   perf-report.txt   where the time goes, function by function (a sampling profiler)
//   perf-stat.txt     what the processor was doing (hardware counters)
//   callgrind.txt     how many instructions, and how many calls (an exact counter)
// Read them (the comments at the top of each file explain the columns), then answer the six questions. Press Run to see your answers printed.

enum class Bottleneck { Compute, Memory, BranchMispredictions };

// 1. Which function spends the most time in its OWN code? (perf-report.txt: the "Self" column.) Return its name exactly as the file spells it.
inline const char* hottest_by_self_time() { return "?"; }

// 2. What percent of the whole run is spent in process_image AND everything it calls? (The "Children" column.) Round to a whole percent.
inline int percent_in_process_image_and_callees() { return 0; }

// 3. Is the program limited by arithmetic, by waiting for memory, or by mispredicted branches? (perf-stat.txt: instructions per cycle, cache misses, branch misses.)
inline Bottleneck bottleneck_from_perf_stat() { return Bottleneck::Compute; }

// 4. Which function is CALLED the most often? (callgrind.txt.) Is it also the one that costs the most?
inline const char* most_called_function() { return "?"; }

// 5. Amdahl's law: if a part of the run that takes a fraction p of the time becomes s times faster, the whole run takes (1 - p) + p / s of the old time,
//    so the speedup is 1 / ((1 - p) + p / s). What is the best overall speedup if blur_row (use its share of the time) becomes 10 times faster?
inline double best_speedup_if_blur_row_is_10x_faster() { return 1.0; }

// 6. A teammate profiled a build made without optimization (-O0, "debug"). Is that a reliable guide to where the optimized program (-O2) spends its time?
inline bool debug_build_profile_is_reliable() { return true; }
