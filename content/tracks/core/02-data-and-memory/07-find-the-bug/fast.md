# Fast path

Six functions, one classic bug each. Find and fix them (`main.cpp` and `bugs.hpp`; keep the signatures).

1. `fill_squares`: off-by-one write (`<=` instead of `<`).
2. `Label`: a `const std::string&` member bound to a temporary dangles. Own a `std::string`.
3. `take_and_release`: reads `*p` after `delete p`. Copy first.
4. `first_after_growth`: a pointer to `v[0]` taken before `push_back` dangles if the vector reallocates. Read it after.
5. `count_positive`: `int count;` is uninitialized. `= 0`.
6. `make_entry`: an `Entry` holding a `string_view` of a local string dangles. Own a `std::string`.

The runner poisons freed memory with `0xDD`, pre-fills the stack with `0x5A`, and puts a canary after arrays, so these bugs fail loudly.
