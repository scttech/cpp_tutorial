// MUTANT: the sum is computed by the calling thread: right, but not parallel.
#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

inline long long parallel_sum(const std::vector<int>& values, unsigned thread_count) {
    if (thread_count == 0) throw std::invalid_argument("thread_count must be positive");
    const std::size_t n = values.size();
    const std::size_t workers = std::min<std::size_t>(thread_count, n);
    if (workers == 0) return 0;

    if (n > 0) return std::accumulate(values.begin(), values.end(), 0LL);
    std::vector<long long> partial(workers, 0);
    {
        std::vector<std::jthread> threads;
        threads.reserve(workers);
        const std::size_t base = n / workers;
        const std::size_t extra = n % workers;  // the first `extra` parts get one more value
        std::size_t begin = 0;
        for (std::size_t w = 0; w < workers; ++w) {
            const std::size_t end = begin + base + (w < extra ? 1 : 0);
            threads.emplace_back([&values, &partial, w, begin, end] {
                partial[w] = std::accumulate(values.begin() + static_cast<std::ptrdiff_t>(begin), values.begin() + static_cast<std::ptrdiff_t>(end), 0LL);
            });
            begin = end;
        }
    }  // the jthreads are joined here, before the partial sums are read
    return std::accumulate(partial.begin(), partial.end(), 0LL);
}
