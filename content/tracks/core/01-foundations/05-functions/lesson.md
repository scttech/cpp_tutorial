# Functions

```cpp
int add(int a, int b) {
    return a + b;
}
```

A function has a return type, a name, a parameter list, and a body. A function must be **declared before it is
used**. If you need to call a function that is defined further down, put a declaration (a *prototype*) above:

```cpp
int add(int a, int b);   // declaration: "this exists"
// ... code that calls add() ...
int add(int a, int b) { return a + b; }   // definition
```

## Passing arguments: copy, reference, `const&`

By default arguments are **copied**. That is cheap for an `int`, but for a `std::vector` or `std::string` it copies
every element, silently:

```cpp
double mean(std::vector<double> v);            // copies the whole vector on every call
void   fill(std::vector<double>& v);           // reference: can modify the caller's vector
double mean(const std::vector<double>& v);     // const reference: no copy, and read-only
```

Rules of thumb for parameters:
- small, cheap types (`int`, `double`, `char`, pointers): pass by value;
- large types you only read: `const T&`;
- things you intend to modify for the caller: `T&` (and make that obvious in the name).

## Overloading

Several functions may share a name if their parameter types differ. The compiler picks the best match:

```cpp
std::string kind_of(int)    { return "int"; }
std::string kind_of(double) { return "double"; }

kind_of(3);      // "int"
kind_of(3.0);    // "double"
kind_of('a');    // "int"    (char is *promoted* to int: that is a better match than converting to double)
kind_of(3.0f);   // "double" (float is promoted to double)
```

Return type alone cannot distinguish overloads.

## Default arguments

```cpp
std::string greet(const std::string& name, const std::string& punct = "!");
greet("Ada");        // "Ada!"
greet("Ada", "?");   // "Ada?"
```

Defaults go on the **declaration** (usually in a header) and are filled in **at the call site**, using whatever
declaration the caller can see. They are only allowed on trailing parameters.

## Your task

Implement these in `main.cpp` and keep the signatures exactly as shown:

| Function | Behavior |
|---|---|
| `double mean(const std::vector<double>&)` | Arithmetic mean. `0.0` for an empty vector. |
| `double median(const std::vector<double>&)` | The middle value; for an even count, the average of the two middle values. `0.0` for an empty vector. **Must not modify the caller's vector** (that is why it takes `const&`: you will need a sorted *copy* inside). |
| `int mode(const std::vector<int>&)` | The most frequent value. On a tie, the **smallest** of the tied values. `0` for an empty vector. |
| `std::string repeat(const std::string& s, int times = 2, const std::string& sep = "")` | `s` repeated `times` times with `sep` *between* copies (not after the last). `times <= 0` gives `""`. |
| `std::string kind_of(int)` and `std::string kind_of(double)` | Overloads returning `"int"` and `"double"`. |

The `main` in the starter already calls `repeat` with one, two and three arguments, so your default arguments are
exercised by the program output as well.
