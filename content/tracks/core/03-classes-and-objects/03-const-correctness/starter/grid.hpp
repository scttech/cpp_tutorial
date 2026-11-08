#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// A rectangular grid of ints. Nothing here is `const` yet, so a `const Grid&` cannot do anything. Make the class
// const-correct (see the TODOs).
class Grid {
public:
    Grid(std::size_t rows, std::size_t cols, int fill_value = 0) : rows_(rows), cols_(cols), cells_(rows * cols, fill_value) {
        if (rows == 0 || cols == 0) throw std::invalid_argument("Grid: rows and cols must each be at least 1");
    }

    // TODO: these only read. Make them const member functions.
    std::size_t rows() { return rows_; }
    std::size_t cols() { return cols_; }

    // TODO: add a const overload that returns a const reference (read-only access for a const Grid).
    // TODO: the non-const one may be written through, so it must also throw the cached total away (see total()).
    int& at(std::size_t r, std::size_t c) {
        check(r, c);
        return cells_[r * cols_ + c];
    }

    // TODO: fill changes every cell: the cached total is no longer valid afterwards.
    void fill(int value) { std::fill(cells_.begin(), cells_.end(), value); }

    // TODO: make this const, and cache the result: compute the sum only when there is no cached value (and then count
    // it in recomputations_). The cache has to be `mutable` so that a const function may fill it.
    long long total() {
        long long sum = 0;
        for (int v : cells_) sum += v;
        return sum;
    }
    std::size_t recomputations() { return recomputations_; }

    // TODO: const
    std::size_t count(int value) { return static_cast<std::size_t>(std::count(cells_.begin(), cells_.end(), value)); }

    // TODO: const
    std::string row_text(std::size_t r) {
        check(r, 0);
        std::string text;
        for (std::size_t c = 0; c < cols_; ++c) {
            if (c > 0) text += ' ';
            text += std::to_string(cells_[r * cols_ + c]);
        }
        return text;
    }

private:
    // TODO: const
    void check(std::size_t r, std::size_t c) {
        if (r >= rows_ || c >= cols_) throw std::out_of_range("Grid: cell is outside the grid");
    }

    std::size_t rows_;
    std::size_t cols_;
    std::vector<int> cells_;
    std::optional<long long> cache_;  // TODO: the cached total
    std::size_t recomputations_ = 0;  // how often the total was really computed
};

// TODO: this only reads the grid, so take it by const reference (not a copy, not a non-const reference).
// (It is `inline` because it is defined in a header: otherwise two files including this header would define it twice.)
inline long long row_sum(Grid& g, std::size_t r) {
    long long sum = 0;
    for (std::size_t c = 0; c < g.cols(); ++c) sum += g.at(r, c);
    return sum;
}
