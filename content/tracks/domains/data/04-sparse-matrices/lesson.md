# Sparse Vectors and CSR Matrices

> **You're on the scientific computing team.** A finite-element mesh with a million nodes has a million-by-million stiffness matrix, with five to ten non-zero entries per row. Stored densely that is 8 terabytes; stored sparsely it is 80 megabytes, and multiplying it by a vector takes 10 million operations instead of 10^12. The same shape appears in PageRank (one row per web page), recommender systems (users by items), text search (documents by terms) and graph algorithms. **CSR**, *compressed sparse row*, is the layout almost every library uses for it.

## The three arrays

```
        [ 1 . 2 . . ]       values  = [1 2 | 3 4 | 5 6]
  A  =  [ . . . . . ]       col_idx = [0 2 | 1 3 | 0 4]
        [ . 3 . 4 . ]       row_ptr = [0, 2, 2, 4, 6]
        [ 5 . . . 6 ]
```

Row `r` lives in positions `row_ptr[r] .. row_ptr[r+1]` of `values` and `col_idx`. An empty row is just two equal `row_ptr` entries. Looking up `A(r, c)` is a **binary search** in that row's slice of `col_idx` (that is why indices must be sorted), and `y = A x` is one pass over the arrays:

```
for r: for k in row_ptr[r] .. row_ptr[r+1]: y[r] += values[k] * x[col_idx[k]]
```

## Canonical form is the whole game

If the invariants (columns **strictly increasing** in a row, **no stored zeros**) hold, then `at`, `transpose`, `plus` and equality are simple, and two matrices with the same entries have identical arrays. Break them (a duplicate column here, an explicit `0.0` there) and every algorithm needs defensive code. So `from_triplets` is the one place that does the hard work:

- **Out-of-range** entries are refused, and so are absurd dimensions: `row_ptr` has `rows + 1` entries, so `rows = SIZE_MAX` would overflow, and `rows = 10^12` would try to allocate 8 TB. Check before allocating.
- **Duplicates are summed** (that is what "assemble the stiffness matrix" means), **in the order given**: floating-point addition is not associative, and reproducible results need a defined order. Use `std::stable_sort`.
- **Zero sums are dropped.** `x` and `-x` leave nothing, and so does an explicit `0.0`.

## Transpose without sorting

The naive transpose builds triplets and sorts them: `O(nnz log nnz)`. Counting does it in `O(nnz + cols)`: count the entries in each column (this gives the new `row_ptr` after a prefix sum), then walk the old rows in order and drop each entry into its new row at the next free slot. Because you visit old rows in ascending order, each new row receives its column indices already sorted.

## Merging

`plus` is the same two-pointer merge as `std::set_union`: walk the two sorted column lists of a row together; an entry in only one side is scaled and copied, one in both is combined, and an entry that cancels (`x + (-x)`) is dropped. `SparseVector::dot` is the same walk, keeping only the indices both have.

## The tiers

- **Works**: the CSR arrays of a known matrix, duplicate/zero/NaN rules, ordered summation, refusals and dimension limits, `at`, the products, transpose, `plus`, sparse vectors.
- **Robust**: 400 random matrices against a dense reference for every operation (including bit-exact floating-point sums), degenerate shapes, garbage triplets.
- **Fast**: zero allocations in the kernels, at most five for a transpose; a **524,288 x 524,288** matrix with 1.5 million entries (a dense one would not fit in memory, so any `O(rows x cols)` code fails here); 200000 look-ups in a row of a million entries.
- **Clean**: no raw memory or exceptions, no `map`/`set`/`list`, no linear `find`, and a binary search must be present.
