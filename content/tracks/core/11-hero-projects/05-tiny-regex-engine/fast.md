# Fast path: what's different in C++

- **`std::regex` is not the tool.** The standard library's engine is a backtracker with a recursive implementation: slow, and able to overflow the stack on long input. Production C++ uses RE2, Hyperscan or CTRE (compile-time regex); they all rest on the automata you are writing. Knowing why `std::regex` is avoided is part of knowing C++.
- **`std::bitset<256>`** is a set of bytes in 32 bytes: `test(c)` is one instruction. The right data structure for a character class, and a good example of a value type that is cheap to copy and compare.
- **Sparse sets without clearing.** `seen[state] == position` marks membership for one step; moving to the next position needs no clear, because the stamp changes. The same trick is used in the Linux scheduler and in the Briggs-Torczon sparse set.
- **No recursion in the hot loop.** The epsilon closure uses an explicit `std::vector` stack: no stack overflow on a pattern with 5000 `a?` in a row, and the vector is reused (cleared, not reallocated).
- **`const State&` from `nfa.at(i)`** is a reference into the vector; it stays valid because nothing adds states while matching. References into containers are invalidated by growth, a classic C++ bug when *building* (`nfa.edit(loop).out = body` after more states were added is fine because it re-indexes).
- **Building backwards** (`compile(node, next)`) is a continuation-passing style: the "rest of the program" is an argument, so no dangling arrows need patching.
