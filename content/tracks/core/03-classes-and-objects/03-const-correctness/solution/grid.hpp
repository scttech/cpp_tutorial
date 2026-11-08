#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// A rectangular grid of ints, const-correct: everything that only reads is `const`, so a `const Grid&` is fully usable.
class Grid {
public:
    Grid(std::size_t rows, std::size_t cols, int fill_value = 0) : rows_(rows), cols_(cols), cells_(rows * cols, fill_value) {
        if (rows == 0 || cols == 0) throw std::invalid_argument("Grid: rows and cols must each be at least 1");
    }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }

    // Two overloads: the compiler picks the const one for a const Grid and the other one otherwise.
    int& at(std::size_t r, std::size_t c) {
        check(r, c);
        cache_.reset();  // the caller may write through this reference, so a cached total could now be wrong
        return cells_[r * cols_ + c];
    }
    const int& at(std::size_t r, std::size_t c) const {
        check(r, c);
        return cells_[r * cols_ + c];  // read-only: the cache stays valid
    }

    void fill(int value) {
        std::fill(cells_.begin(), cells_.end(), value);
        cache_.reset();
    }

    // Logically const (it does not change what the grid *is*), but it keeps a cache: `mutable` allows that.
    long long total() const {
        if (!cache_) {
            long long sum = 0;
            for (int v : cells_) sum += v;
            cache_ = sum;
            ++recomputations_;
        }
        return *cache_;
    }
    std::size_t recomputations() const { return recomputations_; }

    std::size_t count(int value) const { return static_cast<std::size_t>(std::count(cells_.begin(), cells_.end(), value)); }

    std::string row_text(std::size_t r) const {
        check(r, 0);
        std::string text;
        for (std::size_t c = 0; c < cols_; ++c) {
            if (c > 0) text += ' ';
            text += std::to_string(cells_[r * cols_ + c]);
        }
        return text;
    }

private:
    void check(std::size_t r, std::size_t c) const {
        if (r >= rows_ || c >= cols_) throw std::out_of_range("Grid: cell is outside the grid");
    }

    std::size_t rows_;
    std::size_t cols_;
    std::vector<int> cells_;
    mutable std::optional<long long> cache_;  // the cached total; mutable so that total() const may fill it
    mutable std::size_t recomputations_ = 0;  // how often the total was really computed
};

// A free function that only reads takes a `const Grid&`: no copy, and it promises not to modify the grid.
// (`inline` because this is a header: without it, two files that include it would define the function twice.)
inline long long row_sum(const Grid& g, std::size_t r) {
    long long sum = 0;
    for (std::size_t c = 0; c < g.cols(); ++c) sum += g.at(r, c);
    return sum;
}
