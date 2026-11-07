# References

A **reference** is another name for an object that already exists. It is not a copy and not a pointer you have to
dereference: using the reference *is* using the original.

```cpp
int x = 10;
int& r = x;     // r is an alias for x
r = 20;         // x is now 20
```

A reference must be initialized when it is created, and it can never be re-seated to refer to something else. (An
assignment to `r` assigns to `x`.) There is no such thing as a null reference.

## Reference parameters

By default a function gets **copies** of its arguments. A reference parameter lets it work on the caller's object:

```cpp
void add_one(int& n) { ++n; }        // modifies the caller's variable
int total = 5;
add_one(total);                      // total is 6
```

- `T&` : read **and write** the caller's object. Make it obvious in the function name that it modifies.
- `const T&` : read the caller's object **without copying it**, and without being allowed to change it. This is how you
  pass big objects (strings, vectors) cheaply. It also binds to temporaries: `print(std::string("hi"))` is fine.
- `T` (by value) : an independent copy. Right for small types like `int` and `double`.

Because the reference is the same object, **two parameters can refer to the same variable**. `swap_values(x, x)` is legal.
Code that assumes the two parameters are different objects can break: a clever "swap with XOR" trick sets a variable to
zero when you swap it with itself.

## Returning references

A function can return a reference to something that **outlives the call**: an element of the vector it was given, or
one of its own arguments. The caller can then read or write it:

```cpp
int& first(std::vector<int>& v) { return v[0]; }
first(v) = 99;                       // changes v[0]
```

The danger is returning a reference to something that dies when the function returns:

```cpp
int& oops() {
    int local = 5;
    return local;          // local is destroyed at the closing brace: the reference dangles (undefined behavior)
}
```

The compiler usually warns about this. The rule: only return a reference to an object that existed before the call
(a parameter, a member, an element of a container passed in) and will still exist after it. Returning a reference to a
**temporary** argument is the same bug in disguise: `const int& m = larger(3, 4);` leaves `m` dangling once the statement ends,
because `3` and `4` were temporaries.

## References in range-for

```cpp
for (int x : v)        { x = 0; }    // x is a COPY of each element: v is unchanged
for (int& x : v)       { x = 0; }    // x is each element: v is zeroed
for (const auto& s : words) { ... }  // read-only, no copies of the strings
```

Forgetting the `&` when you meant to modify is one of the most common reference bugs: the code compiles and silently
does nothing.

## Your task

| Function | Behavior |
|---|---|
| `void swap_values(int&, int&)` | Exchange two variables (also when both are the same variable). |
| `void rotate3(int& a, int& b, int& c)` | `a` gets `b`, `b` gets `c`, `c` gets the original `a`. |
| `const int& larger(const int& a, const int& b)` | Return a **reference** to the larger argument; `a` if equal. |
| `int& largest_in(std::vector<int>& v)` | A reference to the largest element (the first one on a tie). `v` is never empty. |
| `void clamp_all(std::vector<int>& v, int lo, int hi)` | Limit every element to `lo..hi`, in place. |
| `int& element_or(std::vector<int>& v, std::size_t i, int& fallback)` | A reference to `v[i]` if valid, otherwise to `fallback`. |

The tests check *which object* you return by comparing addresses, so a function that returns a copy of the right
value is not enough.
