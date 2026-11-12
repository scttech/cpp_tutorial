// A sparse matrix in CSR form: only the non-zeros are stored, and y = A x costs one pass over them. Read-only. Press Run.
#include <cstdio>
#include <vector>

#include "sparse.hpp"

int main() {
    // a 4 x 5 matrix with 6 non-zeros (zeros are not stored)
    //   [ 1 . 2 . . ]
    //   [ . . . . . ]
    //   [ . 3 . 4 . ]
    //   [ 5 . . . 6 ]
    const auto m = sparse::Csr::from_triplets(4, 5, {{0, 0, 1}, {0, 2, 2}, {2, 1, 3}, {2, 3, 4}, {3, 0, 5}, {3, 4, 6}});
    if (!m) return 1;
    std::printf("%zu x %zu, %zu stored values\n", m->rows(), m->cols(), m->nnz());
    std::printf("row_ptr:");
    for (std::size_t v : m->row_ptr()) std::printf(" %zu", v);
    std::printf("\ncol_idx:");
    for (std::size_t v : m->col_idx()) std::printf(" %zu", v);
    std::printf("\nvalues :");
    for (double v : m->values()) std::printf(" %g", v);
    std::printf("\n");

    const std::vector<double> x = {1, 1, 1, 1, 1};
    const auto y = m->multiply(x);
    if (y) {
        std::printf("A * [1 1 1 1 1] =");
        for (double v : *y) std::printf(" %g", v);
        std::printf("\n");
    }
    const sparse::Csr t = m->transpose();
    std::printf("transposed: %zu x %zu, A(3,4) = %g and A^T(4,3) = %g\n", t.rows(), t.cols(), m->at(3, 4).value_or(-1), t.at(4, 3).value_or(-1));

    const auto a = sparse::SparseVector::from_pairs(10, {{7, 2.0}, {1, 3.0}, {4, -1.0}});
    const auto b = sparse::SparseVector::from_pairs(10, {{4, 5.0}, {7, 0.5}, {9, 8.0}});
    if (a && b) std::printf("sparse dot product = %g\n", a->dot(*b).value_or(-1));
}
