// Prints your answers (read-only). Open perf-report.txt, perf-stat.txt and callgrind.txt next to this file, answer in answers.hpp, press Run.
#include <iostream>

#include "answers.hpp"

int main() {
    std::cout << "1. most time in its own code (perf-report.txt, Self): " << hottest_by_self_time() << "\n";
    std::cout << "2. percent of the time in process_image and everything it calls: " << percent_in_process_image_and_callees() << "%\n";
    const char* names[] = {"compute-bound", "memory-bound", "branch-misprediction-bound"};
    std::cout << "3. bottleneck according to perf-stat.txt: " << names[static_cast<int>(bottleneck_from_perf_stat())] << "\n";
    std::cout << "4. most calls (callgrind.txt): " << most_called_function() << "\n";
    std::cout << "5. best possible overall speedup if blur_row became 10 times faster: " << best_speedup_if_blur_row_is_10x_faster() << "x\n";
    std::cout << "6. is a profile of an unoptimized (-O0) build a reliable guide to the optimized program? " << (debug_build_profile_is_reliable() ? "yes" : "no") << "\n";
}
