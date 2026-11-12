# A Matrix Library with Strided Views

> **You're on the scientific computing team.** Everything downstream (regression, simulation, image filters, a neural-network layer) is matrix arithmetic, and two decisions made in the first week decide whether the library is pleasant or painful for years: *how a matrix is laid out in memory*, and *whether a "sub-matrix" or a "transpose" is a copy or a view*. NumPy, Eigen, BLAS and every GPU library answer the same way: one flat buffer, plus **strides**.

## One buffer, two strides

A matrix of `rows x cols` is stored **row-major** in one `std::vector<T>`: element `(r, c)` is at `data[r * cols + c]`. A **view** is a pointer plus a shape plus two strides:

```
address of (r, c)  =  ptr + r * row_stride + c * col_stride
```

For a plain matrix `row_stride = cols`, `col_stride = 1`. Now the tricks are free:

| Operation | What changes | Copies? |
|---|---|---|
| `transposed()` | swap `rows`<->`cols` **and** `row_stride`<->`col_stride` | none |
| `block(r0, c0, nr, nc)` | move the pointer by `r0 * rs + c0 * cs`, new shape, **same strides** | none |
| every row/column/diagonal slice | the same idea | none |

This is why a view must be **non-owning** and why `View<const T>` exists: a read-only window you can pass around without being able to modify the matrix, and without paying for a copy.

### `block` must not overflow

`block(1, 0, SIZE_MAX, 1)` asks for "rows 1 to SIZE_MAX". The check `r0 + nr <= rows` **wraps around** (`1 + SIZE_MAX == 0`) and accepts it. Write the checks as `r0 <= rows && nr <= rows - r0`, which cannot overflow. This is the same bug class as the classic `if (offset + length > size)` buffer-overread.

## Multiplying: where the time goes

`C(i,j) = sum over k of A(i,k) * B(k,j)`. The textbook loop order (`i`, `j`, `k`) walks `A` along a row (good: neighbouring addresses) but `B` down a **column**: each step is a whole row (`cols * sizeof(T)` bytes) further on. For a big matrix every single access to `B` misses the cache.

**Cache blocking** (tiling) cuts the three loops into `block x block x block` pieces so that the three tiles in use (a piece each of `A`, `B`, `C`) fit in the cache and are *reused* many times before being evicted. The same idea underlies every serious GEMM implementation (BLAS, oneDNN, cuBLAS).

Two rules make it correct:

- **Edge tiles.** When `n` is not a multiple of the block size the last tile is smaller. Clamp the end of each tile with `std::min(block, n - start)`; the block may also be bigger than the whole matrix (even `SIZE_MAX`), which must simply mean one tile.
- **Same order of additions.** Floating-point addition is not associative. If `(i, j)`'s terms are added in a different order than `multiply()` adds them, the results differ in the last bits and your tests become flaky. Looping the `k`-tiles in ascending order and `k` ascending inside each tile keeps the order identical, and the test checks for **bit-identical doubles**.

## What the tests do

You can't time a cache on a shared one-CPU runner, so the Fast tier uses a **cache model**: the test's element type reports every operand it reads to a simulated 64-line, LRU, fully associative L1. A 96 x 96 product with the textbook loops misses on practically every `B` access; the blocked version must miss at least four times less. It also counts multiplications (exactly `n * k * m`, no tile done twice) and allocations (a view is free; a product allocates only its result).

## The tiers

- **Works**: shape and strides, transposes (shape *and* strides), nested blocks, out-of-range windows, `from_rows`, `identity`, `to_matrix`, known products, the blocked product for several block sizes.
- **Robust**: 400 random shapes with random strided, transposed and cut-out operands against an independent reference, and 12 block sizes up to `SIZE_MAX`; bit-identical doubles; zero-sized matrices; overflow-safe `block`; views of views of transposes.
- **Fast**: zero allocations for views, one for a product; exact multiplication counts; at least 4x fewer cache-line misses.
- **Clean**: no raw memory, exceptions or casts that remove `const`; the `View` class contains no copying.
