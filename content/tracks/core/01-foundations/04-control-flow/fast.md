# Fast path: what's different in C++

- `switch` falls through by default; add `break`, or `[[fallthrough]];` when it is intentional. It only accepts integral/enum types.
- Always brace `if`/`else` bodies. Conditions can declare variables: `if (auto it = m.find(k); it != m.end())`.
- Range-for: `for (const auto& x : c)` (use `&` to avoid copying; drop `const` to mutate).
- `.size()` is **unsigned** (`std::size_t`). `v.size() - 1` on an empty vector wraps to a huge number. Mixing it with `int` triggers `-Wsign-compare`.
- Reverse index loop idiom: `for (std::size_t i = v.size(); i-- > 0;)`.
- `std::vector<bool>` is a packed bit container (a famous oddity); fine for a sieve, but `auto& x = v[0]` does not behave like other vectors.
