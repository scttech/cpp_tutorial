// A small matrix library: owning matrices, strided views, free transposes and a cache-blocked multiply. Read-only. Press Run.
#include <cstdio>
#include <optional>

#include "matrix.hpp"

template <class T>
void print(const char* name, mat::View<const T> v) {
    std::printf("%s (%zu x %zu)\n", name, v.rows(), v.cols());
    for (std::size_t r = 0; r < v.rows(); ++r) {
        for (std::size_t c = 0; c < v.cols(); ++c) std::printf(" %6.1f", static_cast<double>(v(r, c)));
        std::printf("\n");
    }
}

int main() {
    const auto a = mat::Matrix<double>::from_rows({{1, 2, 3}, {4, 5, 6}});
    if (!a) return 1;
    print<double>("A", a->view());
    print<double>("A transposed (a view: no copy)", a->view().transposed());
    const auto sub = a->view().block(0, 1, 2, 2);
    if (sub) print<double>("A, the block at (0,1), 2 x 2", *sub);
    const auto p = mat::multiply(a->view(), a->view().transposed());
    if (p) print<double>("A * A^T", p->view());
    const auto q = mat::multiply_blocked(a->view().transposed(), a->view(), 2);
    if (q) print<double>("A^T * A, blocked by 2", q->view());
    std::printf("A * A is %s\n", mat::multiply(a->view(), a->view()) ? "defined" : "not defined (2x3 times 2x3)");
}
